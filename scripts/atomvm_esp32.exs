#!/usr/bin/env elixir

defmodule Main do
  @script_file Path.expand(__ENV__.file)
  @script_name Path.basename(@script_file)

  @default_target "esp32s3"
  @default_idf_rel_path "esp/esp-idf"
  @default_atomvm_rel_path "atomvm/AtomVM"

  @atomvm_esp32_rel_path "src/platforms/esp32"
  @components_dirname "components"
  @sdkconfig_defaults_filename "sdkconfig.defaults"
  @idf_export_sh_filename "export.sh"

  @boot_avm_rel_path "build/libs/esp32boot/elixir_esp32boot.avm"

  @default_host_build_dirname "build"
  @default_platform_build_rel_path Path.join(@atomvm_esp32_rel_path, "build")

  @atomvm_git_url "https://github.com/atomvm/AtomVM.git"

  # Used only for the initial shallow clone.
  @atomvm_clone_branch "main"

  # Default AtomVM version (branch name, tag name, or full commit SHA).
  # Override with: --atomvm-ref REF or ATOMVM_REF=REF
  #
  # Examples:
  #   @atomvm_default_ref "main"
  #   @atomvm_default_ref "v0.6.6"
  #   @atomvm_default_ref "209835dce092c12afce6e520f30c1dece9483708"
  @atomvm_default_ref @atomvm_clone_branch

  @sdkconfig_managed_settings [
    ~s(CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions-elixir.csv"),
    "CONFIG_I2C_SKIP_LEGACY_CONFLICT_CHECK=y"
  ]

  def main(argv) do
    case argv do
      [] ->
        usage()
        System.halt(2)

      ["-h"] ->
        usage()
        System.halt(0)

      ["--help"] ->
        usage()
        System.halt(0)

      [command | rest] ->
        {options, extra_args, invalid} =
          OptionParser.parse(rest,
            strict: [
              atomvm_repo: :string,
              atomvm_ref: :string,
              allow_dirty: :boolean,
              idf_dir: :string,
              target: :string,
              port: :string,
              help: :boolean
            ],
            aliases: [h: :help, p: :port]
          )

        cond do
          invalid != [] ->
            {flag, _value} = List.first(invalid)
            die("Unknown option: #{flag} (use --help)")

          extra_args != [] and command != "mkimage" ->
            die("Unexpected positional arguments: #{Enum.join(extra_args, " ")}")

          Keyword.get(options, :help, false) ->
            usage()
            System.halt(0)

          true ->
            run_command(command, options, extra_args)
        end
    end
  end

  defp run_command(command, options, extra_args) do
    this_repo_root = repo_root()

    atomvm_repo_override = Keyword.get(options, :atomvm_repo, "")
    allow_dirty = Keyword.get(options, :allow_dirty, false) or truthy_env?("ATOMVM_ALLOW_DIRTY")
    idf_dir_override = Keyword.get(options, :idf_dir, "")
    target = Keyword.get(options, :target, @default_target)
    port = Keyword.get(options, :port, "")
    override_was_set = atomvm_repo_override != ""

    idf_dir = resolve_idf_dir(idf_dir_override)
    {atomvm_root, esp32_dir} = resolve_atomvm_paths(this_repo_root, atomvm_repo_override)
    {atomvm_ref, atomvm_ref_source} = resolve_atomvm_ref(options)
    port_display = if port == "", do: "(not set)", else: port

    case command do
      "info" ->
        info_cmd(
          this_repo_root,
          atomvm_root,
          esp32_dir,
          idf_dir,
          target,
          port_display,
          override_was_set,
          atomvm_ref,
          atomvm_ref_source,
          allow_dirty
        )

      "install" ->
        install_cmd(
          this_repo_root,
          atomvm_root,
          esp32_dir,
          idf_dir,
          target,
          port,
          override_was_set,
          atomvm_ref,
          atomvm_ref_source,
          allow_dirty
        )

      "monitor" ->
        monitor_cmd(esp32_dir, idf_dir, port)

      "mkimage" ->
        mkimage_cmd(
          this_repo_root,
          atomvm_root,
          esp32_dir,
          idf_dir,
          target,
          override_was_set,
          atomvm_ref,
          atomvm_ref_source,
          allow_dirty,
          extra_args
        )

      "clean" ->
        clean_cmd(atomvm_root, esp32_dir)

      _ ->
        usage()
        die("Unknown command: #{command}")
    end
  end

  defp usage do
    IO.puts("""
    Usage:
      #{@script_name} <command> [options]
      #{@script_name} mkimage [options] [-- mkimage args...]

    Commands:
      info      Print resolved paths and basic checks (no changes)
      install   Ensure AtomVM exists, pin version, link component, patch config, build + flash firmware
      monitor   Attach serial monitor (idf.py monitor)
      mkimage   Build a custom AtomVM release image for this project
      clean     Remove AtomVM ESP32 platform build directory

    Common options:
      --atomvm-repo PATH       AtomVM repo root (or wrapper containing AtomVM/)
      --atomvm-ref REF         AtomVM ref: branch/tag/full SHA
                              default: #{@atomvm_default_ref}
                              env: ATOMVM_REF
      --allow-dirty            Allow pinning even if AtomVM repo has tracked local changes
                              env: ATOMVM_ALLOW_DIRTY=1
      --idf-dir PATH           ESP-IDF root (contains export.sh). Optional.
      --target TARGET          esp32 / esp32s3 / etc (default: #{@default_target})
      --port PORT              Serial device (required for install/monitor)
      -h, --help               Show help

    mkimage notes:
      - Host build dir is inferred as: <atomvm_repo>/#{@default_host_build_dirname}
      - Platform build dir is inferred as: <atomvm_repo>/#{@default_platform_build_rel_path}
      - Boot AVM is inferred as: <atomvm_repo>/#{@boot_avm_rel_path}
      - Extra positional arguments are forwarded to mkimage.sh unchanged.

    clean notes:
      - Removes only the ESP32 platform build directory:
          <atomvm_repo>/#{@default_platform_build_rel_path}
      - Does not remove the host build directory.

    Examples:
      #{@script_name} info
      #{@script_name} install --target esp32s3 --port /dev/ttyACM0
      #{@script_name} install --allow-dirty --target esp32s3 --port /dev/ttyACM0
      #{@script_name} install --atomvm-ref v0.6.6 --target esp32s3 --port /dev/ttyACM0
      #{@script_name} install --atomvm-ref 209835dce092c12afce6e520f30c1dece9483708 --target esp32s3 --port /dev/ttyACM0
      #{@script_name} monitor --port /dev/ttyACM0
      #{@script_name} mkimage
      #{@script_name} mkimage --target esp32s3
      #{@script_name} mkimage -- --main build/my_app.avm
      #{@script_name} mkimage -- --main build/my_app.avm --data build/assets.avm
      #{@script_name} clean

    ESP-IDF discovery (if --idf-dir not provided):
      Uses ESP_IDF_DIR, then IDF_PATH, else defaults to: $HOME/#{@default_idf_rel_path}

    AtomVM pinning:
      - Branch ref (e.g. main): always fetches origin/<branch> with depth=1 before resolving
      - Tag ref (e.g. v0.6.6): resolves tag target commit (stable)
      - SHA ref (40 hex): checks out that commit (stable)
    """)
  end

  defp die(message) do
    IO.puts(:stderr, colorize(:red, "✖ #{message}", bold: true))
    System.halt(1)
  end

  defp say(message) do
    cond do
      String.starts_with?(message, "✔") -> IO.puts(colorize(:green, message))
      String.starts_with?(message, "Next:") -> IO.puts(colorize(:yellow, message))
      true -> IO.puts(message)
    end
  end

  defp run!(cmd, args, opts \\ []) do
    display_override = Keyword.get(opts, :display)
    display = display_override || [cmd | Enum.map(args, &shell_display/1)] |> Enum.join(" ")
    IO.puts(colorize(:cyan, "+ #{display}", bold: true))
    clean_opts = Keyword.drop(opts, [:display])

    system_opts =
      [stderr_to_stdout: true, into: IO.stream(:stdio, :line)]
      |> Keyword.merge(clean_opts)

    {_result, status} = System.cmd(cmd, args, system_opts)

    if status != 0 do
      die("Command failed (exit #{status}): #{display}")
    end

    :ok
  end

  defp require_cmd!(cmd) do
    case System.find_executable(cmd) do
      nil -> die("Missing dependency: #{cmd}")
      _ -> :ok
    end
  end

  defp ensure_regular_file!(path, label) do
    if not File.regular?(path) do
      die("#{label} not found: #{path}")
    end
  end

  defp ensure_serial_port_ready!(port) do
    if !File.exists?(port) do
      die("Serial port not found: #{port}")
    end

    case serial_port_busy_details(port) do
      nil ->
        :ok

      details ->
        die("""
        Serial port is busy: #{port}

        #{details}

        Tip:
          Close any serial monitor/tool using the port (idf.py monitor, screen, minicom, picocom, etc.)
          Then retry.
        """)
    end
  end

  defp serial_port_busy_details(port) do
    lsof_output = serial_port_lsof_output(port)
    fuser_output = serial_port_fuser_output(port)

    cond do
      present?(lsof_output) ->
        "Detected by lsof:\n" <> lsof_output

      present?(fuser_output) ->
        "Detected by fuser:\n" <> fuser_output

      true ->
        nil
    end
  end

  defp serial_port_lsof_output(port) do
    if System.find_executable("lsof") do
      case System.cmd("lsof", ["-n", "-w", port], stderr_to_stdout: true) do
        {output, 0} ->
          String.trim(output)

        {_output, _status} ->
          ""
      end
    else
      ""
    end
  end

  defp serial_port_fuser_output(port) do
    if System.find_executable("fuser") do
      case System.cmd("fuser", [port], stderr_to_stdout: true) do
        {output, 0} ->
          String.trim(output)

        {_output, _status} ->
          ""
      end
    else
      ""
    end
  end

  defp script_dir do
    Path.dirname(@script_file)
  end

  defp repo_root do
    Path.expand("..", script_dir())
  end

  defp resolve_idf_dir(""), do: resolve_idf_dir(nil)

  defp resolve_idf_dir(nil) do
    cond do
      present?(System.get_env("ESP_IDF_DIR")) -> Path.expand(System.get_env("ESP_IDF_DIR"))
      present?(System.get_env("IDF_PATH")) -> Path.expand(System.get_env("IDF_PATH"))
      true -> Path.join(System.user_home!(), @default_idf_rel_path)
    end
  end

  defp resolve_idf_dir(override) do
    Path.expand(override)
  end

  defp resolve_atomvm_paths(_this_repo_root, override)
       when is_binary(override) and override != "" do
    override = Path.expand(override)

    cond do
      File.dir?(Path.join(override, @atomvm_esp32_rel_path)) ->
        {override, Path.join(override, @atomvm_esp32_rel_path)}

      File.dir?(Path.join(override, Path.join("AtomVM", @atomvm_esp32_rel_path))) ->
        atomvm_root = Path.join(override, "AtomVM")
        {atomvm_root, Path.join(atomvm_root, @atomvm_esp32_rel_path)}

      true ->
        die("Could not find AtomVM under --atomvm-repo: #{override}")
    end
  end

  defp resolve_atomvm_paths(this_repo_root, _override) do
    normalized = Path.expand(this_repo_root)

    if String.match?(normalized, ~r|/src/platforms/esp32/components/[^/]+$|) do
      esp32_dir = Path.expand("../..", normalized)
      atomvm_root = Path.expand("../../..", esp32_dir)
      {atomvm_root, esp32_dir}
    else
      atomvm_root = Path.join(System.user_home!(), @default_atomvm_rel_path)
      {atomvm_root, Path.join(atomvm_root, @atomvm_esp32_rel_path)}
    end
  end

  defp resolve_atomvm_ref(options) do
    cli = Keyword.get(options, :atomvm_ref, "") |> to_string() |> String.trim()
    env = (System.get_env("ATOMVM_REF") || "") |> String.trim()

    cond do
      present?(cli) -> {cli, "--atomvm-ref"}
      present?(env) -> {env, "ATOMVM_REF"}
      true -> {@atomvm_default_ref, "default(@atomvm_default_ref)"}
    end
  end

  defp ensure_atomvm_repo!(
         atomvm_root,
         override_was_set,
         atomvm_ref,
         atomvm_ref_source,
         allow_dirty
       ) do
    cond do
      override_was_set ->
        if File.dir?(Path.join(atomvm_root, ".git")) do
          :ok
        else
          die("AtomVM repo not found at --atomvm-repo location: #{atomvm_root}")
        end

      File.dir?(Path.join(atomvm_root, ".git")) ->
        say("✔ AtomVM repo exists: #{atomvm_root}")

      File.exists?(atomvm_root) ->
        die("Default AtomVM path exists but is not a git repo: #{atomvm_root}")

      true ->
        require_cmd!("git")
        File.mkdir_p!(Path.dirname(atomvm_root))
        say("Cloning AtomVM into: #{atomvm_root}")

        run!("git", [
          "clone",
          "--filter=blob:none",
          "--depth",
          "1",
          "--branch",
          @atomvm_clone_branch,
          @atomvm_git_url,
          atomvm_root
        ])
    end

    ensure_atomvm_ref!(atomvm_root, atomvm_ref, atomvm_ref_source, allow_dirty)
  end

  defp ensure_atomvm_ref!(atomvm_root, atomvm_ref, atomvm_ref_source, allow_dirty) do
    ref = to_string(atomvm_ref) |> String.trim()

    if ref == "" do
      say("✔ AtomVM ref: tracking #{@atomvm_clone_branch} (no pin configured)")
      :ok
    else
      require_cmd!("git")

      if not allow_dirty and git_tracked_dirty?(atomvm_root) do
        die("""
        AtomVM repo has tracked local changes and cannot be pinned safely:
          #{atomvm_root}

        Tip:
          git -C #{shell_display(atomvm_root)} status
          Commit/stash changes, or use a separate clean clone for builds.

          If you want to bypass this check:
            --allow-dirty
            ATOMVM_ALLOW_DIRTY=1
        """)
      end

      current_sha = git_rev_parse!(atomvm_root, "HEAD")
      desired_sha = resolve_ref_to_commit!(atomvm_root, ref)

      if current_sha == desired_sha do
        say(
          "✔ AtomVM pinned: #{ref} (#{String.slice(desired_sha, 0, 12)}) via #{atomvm_ref_source}"
        )
      else
        say(
          "Pinning AtomVM to: #{ref} (#{String.slice(desired_sha, 0, 12)}) via #{atomvm_ref_source}"
        )

        run!("git", ["checkout", "--detach", desired_sha], cd: atomvm_root)
      end

      :ok
    end
  end

  defp resolve_ref_to_commit!(repo_dir, ref) do
    cond do
      sha40?(ref) ->
        resolve_sha!(repo_dir, ref)

      true ->
        resolve_name_ref!(repo_dir, ref)
    end
  end

  defp sha40?(ref), do: String.match?(ref, ~r/^[0-9a-f]{40}$/)

  defp looks_like_version_tag?(ref) do
    String.match?(ref, ~r/^v?\d+\.\d+(\.\d+)?/)
  end

  defp resolve_sha!(repo_dir, sha) do
    case git_try_rev_parse(repo_dir, "#{sha}^{commit}") do
      {:ok, commit} ->
        commit

      :error ->
        {_, status} =
          System.cmd("git", ["fetch", "--depth", "1", "origin", sha],
            cd: repo_dir,
            stderr_to_stdout: true
          )

        if status != 0, do: die("Could not fetch commit SHA from origin: #{sha}")
        git_rev_parse!(repo_dir, "FETCH_HEAD")
    end
  end

  defp resolve_name_ref!(repo_dir, ref) do
    if looks_like_version_tag?(ref) do
      cond do
        fetch_tag(repo_dir, ref) ->
          git_rev_parse!(repo_dir, "refs/tags/#{ref}^{commit}")

        fetch_branch(repo_dir, ref) ->
          git_rev_parse!(repo_dir, "refs/remotes/origin/#{ref}^{commit}")

        match?({:ok, _}, git_try_rev_parse(repo_dir, "#{ref}^{commit}")) ->
          git_rev_parse!(repo_dir, "#{ref}^{commit}")

        true ->
          die_unknown_ref!(ref)
      end
    else
      cond do
        fetch_branch(repo_dir, ref) ->
          git_rev_parse!(repo_dir, "refs/remotes/origin/#{ref}^{commit}")

        fetch_tag(repo_dir, ref) ->
          git_rev_parse!(repo_dir, "refs/tags/#{ref}^{commit}")

        match?({:ok, _}, git_try_rev_parse(repo_dir, "#{ref}^{commit}")) ->
          git_rev_parse!(repo_dir, "#{ref}^{commit}")

        true ->
          die_unknown_ref!(ref)
      end
    end
  end

  defp fetch_tag(repo_dir, tag) do
    refspec = "refs/tags/#{tag}:refs/tags/#{tag}"

    {_, status} =
      System.cmd("git", ["fetch", "--depth", "1", "origin", refspec],
        cd: repo_dir,
        stderr_to_stdout: true
      )

    status == 0
  end

  defp fetch_branch(repo_dir, branch) do
    refspec = "refs/heads/#{branch}:refs/remotes/origin/#{branch}"

    {_, status} =
      System.cmd("git", ["fetch", "--depth", "1", "origin", refspec],
        cd: repo_dir,
        stderr_to_stdout: true
      )

    status == 0
  end

  defp die_unknown_ref!(ref) do
    die("""
    Could not resolve AtomVM ref: #{ref}

    Expected one of:
      - branch name (e.g. main)
      - tag name (e.g. v0.6.6)
      - full SHA (40 hex chars)
    """)
  end

  defp git_dirty?(repo_dir) do
    {out, status} =
      System.cmd("git", ["status", "--porcelain"],
        cd: repo_dir,
        stderr_to_stdout: true
      )

    status == 0 and String.trim(out) != ""
  end

  defp git_tracked_dirty?(repo_dir) do
    {out, status} =
      System.cmd("git", ["status", "--porcelain=v1", "--untracked-files=no"],
        cd: repo_dir,
        stderr_to_stdout: true
      )

    status == 0 and String.trim(out) != ""
  end

  defp git_try_rev_parse(repo_dir, rev) do
    case System.cmd("git", ["rev-parse", "--verify", rev], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} -> {:ok, String.trim(out)}
      {_out, _} -> :error
    end
  end

  defp git_rev_parse!(repo_dir, rev) do
    case System.cmd("git", ["rev-parse", "--verify", rev], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} ->
        String.trim(out)

      {out, status} ->
        die("git rev-parse failed (#{status}) for #{rev}:\n#{String.trim(out)}")
    end
  end

  defp ensure_atomvm_layout!(atomvm_root, esp32_dir) do
    if not File.dir?(atomvm_root) do
      die("AtomVM repo directory not found: #{atomvm_root}")
    end

    if not File.dir?(esp32_dir) do
      die("AtomVM ESP32 platform dir not found: #{esp32_dir}")
    end
  end

  defp ensure_component_link!(this_repo_root, esp32_dir) do
    name = Path.basename(this_repo_root)
    want = Path.join([esp32_dir, @components_dirname, name])

    File.mkdir_p!(Path.join(esp32_dir, @components_dirname))

    cond do
      symlink?(want) ->
        want_real = canonical_path(want)
        repo_real = canonical_path(this_repo_root)

        cond do
          present?(want_real) and present?(repo_real) and want_real == repo_real ->
            say("✔ component symlink ok: #{want}")

          present?(want_real) and present?(repo_real) ->
            die("Component symlink exists but points elsewhere: #{want} -> #{want_real}")

          true ->
            say("✔ component symlink present: #{want}")
        end

      File.exists?(want) ->
        die("Component path exists but is not a symlink: #{want}")

      true ->
        say("Linking component into: #{want}")

        case File.ln_s(this_repo_root, want) do
          :ok -> :ok
          {:error, reason} -> die("Failed to create symlink: #{want} (#{inspect(reason)})")
        end
    end
  end

  defp patch_sdkconfig_defaults!(esp32_dir, tag) do
    path = Path.join(esp32_dir, @sdkconfig_defaults_filename)

    if !File.regular?(path) do
      die("sdkconfig.defaults not found: #{path}")
    end

    begin_marker = "# --- BEGIN #{tag} defaults (managed) ---"
    end_marker = "# --- END #{tag} defaults ---"

    original_content = File.read!(path)

    lines =
      original_content
      |> split_lines_preserve_empty()
      |> remove_managed_block_lines(begin_marker, end_marker)
      |> trim_trailing_blank_lines()

    managed_block = [begin_marker | @sdkconfig_managed_settings] ++ [end_marker]

    new_lines =
      if lines == [] do
        managed_block
      else
        lines ++ [""] ++ managed_block
      end

    new_content = Enum.join(new_lines, "\n") <> "\n"

    File.cp!(path, "#{path}.bak")
    File.write!(path, new_content)

    say("✔ patched: #{path}")
  end

  defp build_boot_avm_if_needed!(atomvm_root) do
    boot_avm = Path.join(atomvm_root, @boot_avm_rel_path)

    if File.regular?(boot_avm) do
      :ok
    else
      require_cmd!("cmake")
      say("Generating boot AVM (Generic UNIX build)")

      build_dir = Path.join(atomvm_root, "build")
      File.mkdir_p!(build_dir)

      run!("cmake", [".."], cd: build_dir)
      run!("cmake", ["--build", "."], cd: build_dir)

      if !File.regular?(boot_avm) do
        die("boot AVM missing after build: #{boot_avm}")
      end
    end
  end

  defp build_host_tree!(atomvm_root, host_build_dir) do
    require_cmd!("cmake")
    File.mkdir_p!(host_build_dir)
    run!("cmake", ["-S", atomvm_root, "-B", host_build_dir])
    run!("cmake", ["--build", host_build_dir])
  end

  defp build_platform_tree!(idf_dir, esp32_dir, target, platform_build_dir) do
    with_idf_env!(
      idf_dir,
      esp32_dir,
      """
      echo "+ idf.py -B #{sh_escape(platform_build_dir)} set-target #{target}"
      idf.py -B #{sh_escape(platform_build_dir)} set-target #{sh_escape(target)}

      echo "+ idf.py -B #{sh_escape(platform_build_dir)} build"
      idf.py -B #{sh_escape(platform_build_dir)} build
      """
    )
  end

  defp run_mkimage!(esp32_dir, mkimage_script, host_build_dir, boot_avm_path, mkimage_extra_args) do
    args =
      [
        sh_escape(mkimage_script),
        "--build_dir",
        sh_escape(host_build_dir),
        "--boot",
        sh_escape(boot_avm_path)
      ] ++ Enum.map(mkimage_extra_args, &sh_escape/1)

    command = Enum.join(args, " ")

    run!(
      "bash",
      ["-Eeuo", "pipefail", "-c", "cd #{sh_escape(esp32_dir)}\n#{command}"],
      display: "bash -c #{shell_display(command)}"
    )
  end

  defp newest_image_path(platform_build_dir) do
    pattern = Path.join(platform_build_dir, "*.img")

    case Path.wildcard(pattern) do
      [] ->
        nil

      paths ->
        paths
        |> Enum.map(fn path -> {path, File.stat!(path).mtime} end)
        |> Enum.max_by(fn {_path, mtime} -> mtime end)
        |> elem(0)
    end
  end

  defp clean_cmd(atomvm_root, esp32_dir) do
    platform_build_dir = Path.join(atomvm_root, @default_platform_build_rel_path)

    ensure_atomvm_layout!(atomvm_root, esp32_dir)

    if File.dir?(platform_build_dir) do
      say("Removing ESP32 build dir: #{platform_build_dir}")
      File.rm_rf!(platform_build_dir)
      say("✔ clean complete")
    else
      say("ESP32 build dir not present: #{platform_build_dir}")
    end
  end

  # Keep ESP-IDF/Python env isolated in a non-login Bash subshell.
  defp with_idf_env!(idf_dir, workdir, shell_body) do
    export_sh = Path.join(idf_dir, @idf_export_sh_filename)

    ensure_regular_file!(export_sh, "ESP-IDF export.sh")

    if !File.dir?(workdir) do
      die("Workdir not found: #{workdir}")
    end

    script = """
    source #{sh_escape(export_sh)} >/dev/null 2>&1

    if ! command -v idf.py >/dev/null 2>&1; then
      echo "idf.py not found in PATH after sourcing ESP-IDF" >&2
      exit 1
    fi

    cd #{sh_escape(workdir)}
    #{shell_body}
    """

    run!(
      "bash",
      ["-Eeuo", "pipefail", "-c", script],
      display: "bash (ESP-IDF env) in #{workdir}"
    )
  end

  defp info_cmd(
         this_repo_root,
         atomvm_root,
         esp32_dir,
         idf_dir,
         target,
         port_display,
         override_was_set,
         atomvm_ref,
         atomvm_ref_source,
         allow_dirty
       ) do
    component_name = Path.basename(this_repo_root)
    component_path = Path.join([esp32_dir, @components_dirname, component_name])

    say("")
    say("Paths")
    say("- repo_root:   #{this_repo_root}")
    say("- atomvm_root: #{atomvm_root}")
    say("- esp32_dir:   #{esp32_dir}")
    say("- idf_dir:     #{idf_dir}")

    say("")
    say("Config")
    say("- target:      #{target}")
    say("- port:        #{port_display}")
    say("- atomvm_ref:  #{atomvm_ref}")
    say("- ref_source:  #{atomvm_ref_source}")
    say("- allow_dirty: #{allow_dirty}")

    say("")
    say("Checks")

    if File.regular?(Path.join(idf_dir, @idf_export_sh_filename)) do
      say("- ESP-IDF:     export.sh found")
    else
      say("- ESP-IDF:     missing export.sh")
    end

    if File.dir?(Path.join(atomvm_root, ".git")) do
      say("- AtomVM:      ok")

      head =
        case System.cmd("git", ["rev-parse", "HEAD"], cd: atomvm_root, stderr_to_stdout: true) do
          {out, 0} -> String.trim(out)
          _ -> "(unknown)"
        end

      dirty_any = if git_dirty?(atomvm_root), do: "yes", else: "no"
      dirty_tracked = if git_tracked_dirty?(atomvm_root), do: "yes", else: "no"

      say("- AtomVM HEAD: #{head}")
      say("- AtomVM dirty(tracked): #{dirty_tracked}")
      say("- AtomVM dirty(any): #{dirty_any}")
    else
      if override_was_set do
        say("- AtomVM:      missing at --atomvm-repo")
      else
        say("- AtomVM:      missing at default (install will clone)")
      end

      case maybe_resolve_atomvm_ref_without_repo(atomvm_ref) do
        {:ok, sha, note} when is_binary(sha) and sha != "" ->
          say("- AtomVM would use: #{sha}  (#{note})")

        {:ok, nil, note} ->
          say("- AtomVM would use: (not resolved)  (#{note})")

        {:error, reason} ->
          say("- AtomVM would use: (not resolved)  (#{reason})")
      end
    end

    if File.dir?(esp32_dir) do
      say("- ESP32 dir:   ok")
    else
      say("- ESP32 dir:   missing")
    end

    if File.exists?(component_path) do
      say("- Component:   present (#{component_path})")
    else
      say("- Component:   not present under esp32/components")
    end

    if port_display != "(not set)" do
      if File.exists?(port_display) do
        say("- Port:        ok")
      else
        say("- Port:        not found (#{port_display})")
      end
    end

    say("")
    say("Inspect")

    components_dir = Path.join(esp32_dir, @components_dirname)

    if File.dir?(components_dir) do
      say("- components:   #{components_dir}")

      case File.ls(components_dir) do
        {:ok, entries} ->
          entries
          |> Enum.sort()
          |> Enum.each(&IO.puts/1)

        {:error, reason} ->
          say("- (failed to list components: #{inspect(reason)})")
      end
    else
      say("- components:   missing (#{components_dir})")
    end

    say("")

    sdkconfig_defaults = Path.join(esp32_dir, @sdkconfig_defaults_filename)

    if File.regular?(sdkconfig_defaults) do
      say("- sdkconfig.defaults: #{sdkconfig_defaults}")
      content = File.read!(sdkconfig_defaults)
      IO.write(content)

      if not String.ends_with?(content, "\n") do
        IO.puts("")
      end
    else
      say("- sdkconfig.defaults: missing (#{sdkconfig_defaults})")
    end

    say("")
  end

  defp install_cmd(
         this_repo_root,
         atomvm_root,
         esp32_dir,
         idf_dir,
         target,
         port,
         override_was_set,
         atomvm_ref,
         atomvm_ref_source,
         allow_dirty
       ) do
    if port == "" do
      die("--port is required for install (e.g. --port /dev/ttyACM0)")
    end

    ensure_serial_port_ready!(port)

    ensure_atomvm_repo!(atomvm_root, override_was_set, atomvm_ref, atomvm_ref_source, allow_dirty)

    if !File.dir?(esp32_dir) do
      die("AtomVM ESP32 platform dir missing: #{esp32_dir}")
    end

    ensure_component_link!(this_repo_root, esp32_dir)
    patch_sdkconfig_defaults!(esp32_dir, Path.basename(this_repo_root))
    build_boot_avm_if_needed!(atomvm_root)

    say("Building AtomVM firmware")

    with_idf_env!(
      idf_dir,
      esp32_dir,
      """
      echo "+ idf.py fullclean"
      idf.py fullclean

      echo "+ idf.py set-target #{target}"
      idf.py set-target #{sh_escape(target)}

      echo "+ idf.py reconfigure"
      idf.py reconfigure

      echo "+ idf.py build"
      idf.py build
      """
    )

    ensure_serial_port_ready!(port)

    say("Flashing AtomVM firmware")

    boot_avm = Path.join(atomvm_root, @boot_avm_rel_path)

    with_idf_env!(
      idf_dir,
      esp32_dir,
      """
      PORT=#{sh_escape(port)}
      BOOT_AVM=#{sh_escape(boot_avm)}

      echo "+ idf.py -p $PORT flash"
      idf.py -p "$PORT" flash

      if [ ! -f "$BOOT_AVM" ]; then
        echo "boot AVM not found: $BOOT_AVM" >&2
        exit 1
      fi

      if [ -z "${IDF_PATH:-}" ]; then
        echo "IDF_PATH not set (ESP-IDF env not loaded?)" >&2
        exit 1
      fi

      PARTTOOL="$IDF_PATH/components/partition_table/parttool.py"
      if [ ! -f "$PARTTOOL" ]; then
        echo "parttool.py not found: $PARTTOOL" >&2
        exit 1
      fi

      PYTHON="python"
      if ! command -v "$PYTHON" >/dev/null 2>&1; then
        PYTHON="python3"
      fi
      if ! command -v "$PYTHON" >/dev/null 2>&1; then
        echo "python not found in PATH (need python or python3)" >&2
        exit 1
      fi

      PT_BIN="build/partition_table/partition-table.bin"
      if [ ! -f "$PT_BIN" ]; then
        echo "partition table bin not found: $PT_BIN" >&2
        exit 1
      fi

      echo "+ parttool.py write_partition boot.avm"
      "$PYTHON" "$PARTTOOL" \\
        -p "$PORT" \\
        --partition-table-file "$PT_BIN" \\
        write_partition \\
        --partition-name boot.avm \\
        --input "$BOOT_AVM"
      """
    )

    say("✔ install complete")
    say("Next: flash the Elixir app from examples/elixir (mix do clean + atomvm.esp32.flash ...)")
  end

  defp monitor_cmd(esp32_dir, idf_dir, port) do
    if port == "" do
      die("--port is required for monitor (e.g. --port /dev/ttyACM0)")
    end

    ensure_serial_port_ready!(port)

    say("Starting serial monitor")

    with_idf_env!(
      idf_dir,
      esp32_dir,
      """
      idf.py -p #{sh_escape(port)} monitor
      """
    )
  end

  defp mkimage_cmd(
         this_repo_root,
         atomvm_root,
         esp32_dir,
         idf_dir,
         target,
         override_was_set,
         atomvm_ref,
         atomvm_ref_source,
         allow_dirty,
         mkimage_extra_args
       ) do
    host_build_dir = Path.join(atomvm_root, @default_host_build_dirname)
    platform_build_dir = Path.join(atomvm_root, @default_platform_build_rel_path)
    boot_avm_path = Path.join(atomvm_root, @boot_avm_rel_path)
    mkimage_script = Path.join(platform_build_dir, "mkimage.sh")

    ensure_atomvm_repo!(atomvm_root, override_was_set, atomvm_ref, atomvm_ref_source, allow_dirty)
    ensure_atomvm_layout!(atomvm_root, esp32_dir)
    ensure_component_link!(this_repo_root, esp32_dir)
    patch_sdkconfig_defaults!(esp32_dir, Path.basename(this_repo_root))
    ensure_regular_file!(Path.join(idf_dir, @idf_export_sh_filename), "ESP-IDF export.sh")

    say("Building AtomVM host tree")
    build_host_tree!(atomvm_root, host_build_dir)

    say("Building AtomVM ESP32 platform")
    build_platform_tree!(idf_dir, esp32_dir, target, platform_build_dir)

    ensure_regular_file!(boot_avm_path, "boot AVM")
    ensure_regular_file!(mkimage_script, "mkimage.sh")

    say("Creating release image")

    run_mkimage!(
      esp32_dir,
      mkimage_script,
      host_build_dir,
      boot_avm_path,
      mkimage_extra_args
    )

    image_path = newest_image_path(platform_build_dir)

    cond do
      image_path == nil ->
        die("mkimage.sh finished, but no .img file was found in #{platform_build_dir}")

      true ->
        say("✔ release image ready")
        IO.puts(image_path)
    end
  end

  defp split_lines_preserve_empty(content) do
    content
    |> String.split(~r/\R/, trim: false)
    |> drop_final_empty_line_if_from_terminal_newline(content)
  end

  defp drop_final_empty_line_if_from_terminal_newline(lines, content) do
    if String.ends_with?(content, "\n") or String.ends_with?(content, "\r\n") do
      case Enum.reverse(lines) do
        ["" | rest] -> Enum.reverse(rest)
        _ -> lines
      end
    else
      lines
    end
  end

  defp remove_managed_block_lines(lines, begin_marker, end_marker) do
    {result, in_block?} =
      Enum.reduce(lines, {[], false}, fn line, {acc, skipping?} ->
        cond do
          line == begin_marker -> {acc, true}
          line == end_marker and skipping? -> {acc, false}
          skipping? -> {acc, true}
          true -> {[line | acc], false}
        end
      end)

    if in_block? do
      die("Managed block start found without end marker in sdkconfig.defaults")
    end

    Enum.reverse(result)
  end

  defp trim_trailing_blank_lines(lines) do
    lines
    |> Enum.reverse()
    |> Enum.drop_while(&(String.trim(&1) == ""))
    |> Enum.reverse()
  end

  defp canonical_path(path) do
    cond do
      System.find_executable("realpath") ->
        case System.cmd("realpath", [path], stderr_to_stdout: true) do
          {output, 0} -> String.trim(output)
          _ -> ""
        end

      System.find_executable("readlink") ->
        case System.cmd("readlink", ["-f", path], stderr_to_stdout: true) do
          {output, 0} -> String.trim(output)
          _ -> ""
        end

      true ->
        ""
    end
  end

  defp symlink?(path) do
    case File.lstat(path) do
      {:ok, %File.Stat{type: :symlink}} -> true
      _ -> false
    end
  end

  defp present?(value), do: is_binary(value) and String.trim(value) != ""

  defp shell_display(arg) do
    arg = to_string(arg)

    if String.contains?(arg, [" ", "\t", "\n", "'", "\"", "$", "`", "\\"]) do
      sh_escape(arg)
    else
      arg
    end
  end

  defp sh_escape(value) do
    "'" <> String.replace(to_string(value), "'", ~s('"'"')) <> "'"
  end

  defp colorize(color, text, opts \\ [])

  defp colorize(_color, text, _opts) when not is_binary(text) do
    IO.iodata_to_binary(text)
  end

  defp colorize(color, text, opts) do
    if ansi_enabled?() do
      maybe_bold =
        if Keyword.get(opts, :bold, false) do
          [IO.ANSI.bright()]
        else
          []
        end

      color_code =
        case color do
          :red -> IO.ANSI.red()
          :green -> IO.ANSI.green()
          :yellow -> IO.ANSI.yellow()
          :cyan -> IO.ANSI.cyan()
          _ -> ""
        end

      IO.iodata_to_binary([maybe_bold, color_code, text, IO.ANSI.reset()])
    else
      text
    end
  end

  defp ansi_enabled? do
    IO.ANSI.enabled?() and is_nil(System.get_env("NO_COLOR"))
  end

  defp truthy_env?(name) do
    case System.get_env(name) do
      nil ->
        false

      value ->
        value
        |> String.trim()
        |> String.downcase()
        |> then(&(&1 in ["1", "true", "yes", "y", "on"]))
    end
  end

  # Resolve a ref to a commit SHA without a local repo (info-only).
  # - Branch: uses ls-remote --heads
  # - Tag: uses ls-remote --tags (prefers peeled ^{} when present)
  # - SHA: returns the SHA as-is (not validated)
  defp maybe_resolve_atomvm_ref_without_repo(ref) do
    ref = to_string(ref) |> String.trim()

    cond do
      ref == "" ->
        {:ok, nil, "no AtomVM ref configured"}

      sha40?(ref) ->
        {:ok, ref, "sha (not validated via ls-remote)"}

      System.find_executable("git") == nil ->
        {:error, "git not found (cannot resolve via ls-remote)"}

      true ->
        case ls_remote_head_sha(ref) do
          {:ok, sha} ->
            {:ok, sha, "branch tip from ls-remote"}

          :error ->
            case ls_remote_tag_sha(ref) do
              {:ok, sha} -> {:ok, sha, "tag target from ls-remote"}
              :error -> {:error, "ls-remote could not find branch or tag on origin for: #{ref}"}
            end
        end
    end
  end

  defp ls_remote_head_sha(branch) do
    args = ["ls-remote", "--heads", @atomvm_git_url, "refs/heads/#{branch}"]

    case System.cmd("git", args, stderr_to_stdout: true) do
      {out, 0} ->
        out
        |> String.split("\n", trim: true)
        |> Enum.map(&String.split(&1, "\t"))
        |> Enum.find_value(:error, fn
          [sha, "refs/heads/" <> _] -> {:ok, sha}
          _ -> nil
        end)

      _ ->
        :error
    end
  end

  defp ls_remote_tag_sha(tag) do
    patterns = ["refs/tags/#{tag}^{}", "refs/tags/#{tag}"]
    args = ["ls-remote", "--tags", @atomvm_git_url] ++ patterns

    case System.cmd("git", args, stderr_to_stdout: true) do
      {out, 0} ->
        lines = String.split(out, "\n", trim: true)

        peeled =
          Enum.find_value(lines, fn line ->
            case String.split(line, "\t") do
              [sha, "refs/tags/" <> rest] ->
                if rest == "#{tag}^{}", do: sha, else: nil

              _ ->
                nil
            end
          end)

        direct =
          Enum.find_value(lines, fn line ->
            case String.split(line, "\t") do
              [sha, "refs/tags/" <> rest] ->
                if rest == tag, do: sha, else: nil

              _ ->
                nil
            end
          end)

        cond do
          is_binary(peeled) and peeled != "" -> {:ok, peeled}
          is_binary(direct) and direct != "" -> {:ok, direct}
          true -> :error
        end

      _ ->
        :error
    end
  end
end

Main.main(System.argv())
