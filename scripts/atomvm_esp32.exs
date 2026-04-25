#!/usr/bin/env elixir

# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule Main do
  @script_file Path.expand(__ENV__.file)
  @script_name Path.basename(@script_file)

  @default_target "esp32s3"
  @recommended_idf_version "v5.5"
  @default_atomvm_rel_path "atomvm/AtomVM"

  @atomvm_esp32_rel_path "src/platforms/esp32"
  @components_dirname "components"

  @sdkconfig_defaults_filename "sdkconfig.defaults"
  @sdkconfig_defaults_template_filename "sdkconfig.defaults.in"
  @project_sdkconfig_defaults_filename ".atomvm_esp32.sdkconfig.defaults"

  @boot_avm_rel_path "build/libs/esp32boot/elixir_esp32boot.avm"
  @default_host_build_dirname "build"
  @default_platform_build_rel_path Path.join(@atomvm_esp32_rel_path, "build")

  @atomvm_git_url "https://github.com/atomvm/AtomVM.git"
  @atomvm_clone_branch "main"
  @atomvm_default_ref @atomvm_clone_branch

  @project_sdkconfig_defaults [
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
    repo_root = repo_root()
    atomvm_root = resolve_atomvm_root(repo_root, Keyword.get(options, :atomvm_repo, ""))
    esp32_dir = Path.join(atomvm_root, @atomvm_esp32_rel_path)
    target = Keyword.get(options, :target, @default_target)
    port = Keyword.get(options, :port, "")
    port_display = if port == "", do: "(not set)", else: port
    override_was_set = present?(Keyword.get(options, :atomvm_repo, ""))
    allow_dirty = Keyword.get(options, :allow_dirty, false) or truthy_env?("ATOMVM_ALLOW_DIRTY")
    {atomvm_ref, atomvm_ref_source} = resolve_atomvm_ref(options)

    shared = %{
      repo_root: repo_root,
      atomvm_root: atomvm_root,
      esp32_dir: esp32_dir,
      target: target,
      port: port,
      port_display: port_display,
      override_was_set: override_was_set,
      atomvm_ref: atomvm_ref,
      atomvm_ref_source: atomvm_ref_source,
      allow_dirty: allow_dirty
    }

    case command do
      "clean" ->
        clean_cmd(shared)

      _ ->
        eim_info = resolve_eim_env_info!()
        shared = Map.merge(shared, eim_info)

        case command do
          "info" ->
            info_cmd(shared)

          "install" ->
            ensure_supported_idf_version!(shared)
            install_cmd(shared)

          "monitor" ->
            ensure_supported_idf_version!(shared)
            monitor_cmd(shared)

          "mkimage" ->
            ensure_supported_idf_version!(shared)
            mkimage_cmd(shared, extra_args)

          _ ->
            usage()
            die("Unknown command: #{command}")
        end
    end
  end

  defp usage do
    IO.puts("""
    Usage:
      #{@script_name} <command> [options]
      #{@script_name} mkimage [options] [-- mkimage args...]

    Commands:
      info      Print resolved paths and basic checks (no changes)
      install   Ensure AtomVM exists, pin version, link component, write project config, build + flash firmware
      monitor   Attach serial monitor (idf.py monitor via EIM)
      mkimage   Build a custom AtomVM release image for this project
      clean     Remove AtomVM ESP32 platform build directory

    Common options:
      --atomvm-repo PATH       AtomVM repo root (or wrapper containing AtomVM/)
      --atomvm-ref REF         AtomVM ref: branch/tag/full SHA
                              default: #{@atomvm_default_ref}
                              env: ATOMVM_REF
      --allow-dirty            Allow pinning even if AtomVM repo has tracked local changes
                              env: ATOMVM_ALLOW_DIRTY=1
      --target TARGET          esp32 / esp32s3 / etc (default: #{@default_target})
      --port PORT              Serial device (required for install/monitor)
      -h, --help               Show help

    Workflow:
      - This script assumes ESP-IDF is managed by EIM.
      - It is currently tuned for AtomVM ESP32 builds with #{@recommended_idf_version}.
      - Install/select the recommended version first:
          eim install -i #{@recommended_idf_version}
          eim select #{@recommended_idf_version}
      - The script runs idf.py through EIM automatically.
      - No activation script sourcing is required.

    mkimage notes:
      - Host build dir is inferred as: <atomvm_repo>/#{@default_host_build_dirname}
      - Platform build dir is inferred as: <atomvm_repo>/#{@default_platform_build_rel_path}
      - Project SDKCONFIG override file is written to:
          <repo_root>/#{@project_sdkconfig_defaults_filename}
      - Extra positional arguments are forwarded to mkimage.sh unchanged.

    clean notes:
      - Removes only the ESP32 platform build directory:
          <atomvm_repo>/#{@default_platform_build_rel_path}
      - Does not remove the host build directory.

    Examples:
      eim install -i #{@recommended_idf_version}
      eim select #{@recommended_idf_version}
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

    AtomVM pinning:
      - Branch ref (e.g. main): always fetches origin/<branch> with depth=1 before resolving
      - Tag ref (e.g. v0.6.6): resolves tag target commit (stable)
      - SHA ref (40 hex): checks out that commit (stable)
    """)
  end

  defp info_cmd(shared) do
    component_path =
      Path.join([shared.esp32_dir, @components_dirname, Path.basename(shared.repo_root)])

    sdkconfig_defaults = Path.join(shared.esp32_dir, @sdkconfig_defaults_filename)

    sdkconfig_defaults_template =
      Path.join(shared.esp32_dir, @sdkconfig_defaults_template_filename)

    project_sdkconfig_overrides =
      Path.join(shared.repo_root, @project_sdkconfig_defaults_filename)

    say("")
    say("Paths")
    say("- repo_root:   #{shared.repo_root}")
    say("- atomvm_root: #{shared.atomvm_root}")
    say("- esp32_dir:   #{shared.esp32_dir}")
    say("- idf_dir:     #{shared.idf_dir}")

    say("")
    say("Config")
    say("- target:      #{shared.target}")
    say("- port:        #{shared.port_display}")
    say("- atomvm_ref:  #{shared.atomvm_ref}")
    say("- ref_source:  #{shared.atomvm_ref_source}")
    say("- allow_dirty: #{shared.allow_dirty}")
    say("- eim_version: #{shared.eim_version}")
    say("- idf_ok:      #{if supported_idf_version?(shared.eim_version), do: "yes", else: "no (expected #{@recommended_idf_version}.x)"}")

    say("")
    say("Checks")
    say("- EIM:         ok")
    say("- ESP-IDF dir: #{if File.dir?(shared.idf_dir), do: "ok", else: "missing"}")

    if File.dir?(Path.join(shared.atomvm_root, ".git")) do
      say("- AtomVM:      ok")
      say("- AtomVM HEAD: #{git_head(shared.atomvm_root)}")
      say("- AtomVM dirty(tracked): #{yes_no(git_tracked_dirty?(shared.atomvm_root))}")
      say("- AtomVM dirty(any): #{yes_no(git_dirty?(shared.atomvm_root))}")
    else
      say(
        "- AtomVM:      #{if shared.override_was_set, do: "missing at --atomvm-repo", else: "missing at default (install will clone)"}"
      )

      case maybe_resolve_atomvm_ref_without_repo(shared.atomvm_ref) do
        {:ok, sha, note} when is_binary(sha) and sha != "" ->
          say("- AtomVM would use: #{sha}  (#{note})")

        {:ok, nil, note} ->
          say("- AtomVM would use: (not resolved)  (#{note})")

        {:error, reason} ->
          say("- AtomVM would use: (not resolved)  (#{reason})")
      end
    end

    say("- ESP32 dir:   #{if File.dir?(shared.esp32_dir), do: "ok", else: "missing"}")

    say(
      "- Component:   #{if File.exists?(component_path), do: "present (#{component_path})", else: "not present under esp32/components"}"
    )

    if shared.port_display != "(not set)" do
      say(
        "- Port:        #{if File.exists?(shared.port_display), do: "ok", else: "not found (#{shared.port_display})"}"
      )
    end

    say("")
    say("Inspect")

    components_dir = Path.join(shared.esp32_dir, @components_dirname)

    if File.dir?(components_dir) do
      say("- components:   #{components_dir}")

      case File.ls(components_dir) do
        {:ok, entries} -> entries |> Enum.sort() |> Enum.each(&IO.puts/1)
        {:error, reason} -> say("- (failed to list components: #{inspect(reason)})")
      end
    else
      say("- components:   missing (#{components_dir})")
    end

    say("")
    print_file_if_present("sdkconfig.defaults.in", sdkconfig_defaults_template)
    say("")
    print_file_if_present("sdkconfig.defaults", sdkconfig_defaults)
    say("")
    print_file_if_present("project sdkconfig overrides", project_sdkconfig_overrides)
    say("")
  end

  defp install_cmd(shared) do
    if shared.port == "", do: die("--port is required for install (e.g. --port /dev/ttyACM0)")

    ensure_serial_port_ready!(shared.port)
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)
    ensure_component_link!(shared.repo_root, shared.esp32_dir)
    sdkconfig_overrides = ensure_project_sdkconfig_defaults!(shared.repo_root)
    build_boot_avm_if_needed!(shared.atomvm_root)

    say("Building AtomVM firmware")

    sdkconfig_defaults = "sdkconfig.defaults;#{sdkconfig_overrides}"

    run_idf!(shared.esp32_dir, ["fullclean"], %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults})

    run_idf!(
      shared.esp32_dir,
      platform_cmake_args(shared.target) ++ ["set-target", shared.target],
      %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults}
    )

    run_idf!(shared.esp32_dir, ["build"], %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults})

    ensure_serial_port_ready!(shared.port)
    say("Flashing AtomVM firmware")

    run_idf!(
      shared.esp32_dir,
      ["-p", shared.port, "flash"],
      %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults}
    )

    say("✔ install complete")
    say("Next: flash the Elixir app from examples/elixir (mix do clean + atomvm.esp32.flash ...)")
  end

  defp monitor_cmd(shared) do
    if shared.port == "", do: die("--port is required for monitor (e.g. --port /dev/ttyACM0)")
    ensure_serial_port_ready!(shared.port)
    say("Starting serial monitor")
    run_idf!(shared.esp32_dir, ["-p", shared.port, "monitor"])
  end

  defp mkimage_cmd(shared, mkimage_extra_args) do
    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)
    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)
    boot_avm_path = Path.join(shared.atomvm_root, @boot_avm_rel_path)
    mkimage_script = Path.join(platform_build_dir, "mkimage.sh")

    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)
    ensure_component_link!(shared.repo_root, shared.esp32_dir)
    sdkconfig_overrides = ensure_project_sdkconfig_defaults!(shared.repo_root)

    say("Building AtomVM host tree")
    build_host_tree!(shared.atomvm_root, host_build_dir)

    say("Building AtomVM ESP32 platform")

    build_platform_tree!(
      shared.esp32_dir,
      shared.target,
      platform_build_dir,
      sdkconfig_overrides,
      release: true
    )

    ensure_regular_file!(boot_avm_path, "boot AVM")
    ensure_regular_file!(mkimage_script, "mkimage.sh")

    say("Creating release image")
    run_mkimage!(shared.esp32_dir, mkimage_script, mkimage_extra_args)

    case newest_image_path(platform_build_dir) do
      nil ->
        die("mkimage.sh finished, but no .img file was found in #{platform_build_dir}")

      image_path ->
        say("✔ release image ready")
        IO.puts(image_path)
    end
  end

  defp clean_cmd(shared) do
    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    if File.dir?(platform_build_dir) do
      say("Removing ESP32 build dir: #{platform_build_dir}")
      File.rm_rf!(platform_build_dir)
      say("✔ clean complete")
    else
      say("ESP32 build dir not present: #{platform_build_dir}")
    end
  end

  defp ensure_atomvm_repo!(shared) do
    ensure_atomvm_repo!(
      shared.atomvm_root,
      shared.override_was_set,
      shared.atomvm_ref,
      shared.atomvm_ref_source,
      shared.allow_dirty
    )
  end

  defp ensure_atomvm_repo!(
         atomvm_root,
         override_was_set,
         atomvm_ref,
         atomvm_ref_source,
         allow_dirty
       ) do
    cond do
      override_was_set and not File.dir?(Path.join(atomvm_root, ".git")) ->
        die("AtomVM repo not found at --atomvm-repo location: #{atomvm_root}")

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
    ref = String.trim(to_string(atomvm_ref))

    cond do
      ref == "" ->
        say("✔ AtomVM ref: tracking #{@atomvm_clone_branch} (no pin configured)")

      not allow_dirty and git_tracked_dirty?(atomvm_root) ->
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

      true ->
        require_cmd!("git")
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
    end
  end

  defp resolve_ref_to_commit!(repo_dir, ref) do
    if sha40?(ref), do: resolve_sha!(repo_dir, ref), else: resolve_name_ref!(repo_dir, ref)
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
    cond do
      looks_like_version_tag?(ref) and fetch_tag(repo_dir, ref) ->
        git_rev_parse!(repo_dir, "refs/tags/#{ref}^{commit}")

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

  defp fetch_tag(repo_dir, tag) do
    refspec = "refs/tags/#{tag}:refs/tags/#{tag}"

    case System.cmd("git", ["fetch", "--depth", "1", "origin", refspec],
           cd: repo_dir,
           stderr_to_stdout: true
         ) do
      {_, 0} -> true
      _ -> false
    end
  end

  defp fetch_branch(repo_dir, branch) do
    refspec = "refs/heads/#{branch}:refs/remotes/origin/#{branch}"

    case System.cmd("git", ["fetch", "--depth", "1", "origin", refspec],
           cd: repo_dir,
           stderr_to_stdout: true
         ) do
      {_, 0} -> true
      _ -> false
    end
  end

  defp resolve_atomvm_ref(options) do
    cli = options |> Keyword.get(:atomvm_ref, "") |> to_string() |> String.trim()
    env = System.get_env("ATOMVM_REF", "") |> String.trim()

    cond do
      present?(cli) -> {cli, "--atomvm-ref"}
      present?(env) -> {env, "ATOMVM_REF"}
      true -> {@atomvm_default_ref, "default(@atomvm_default_ref)"}
    end
  end

  defp resolve_eim_env_info! do
    require_cmd!("eim")

    case System.cmd(
           "eim",
           ["run", ~s|printf '%s\n%s\n' "$ESP_IDF_VERSION" "$IDF_PATH"|],
           stderr_to_stdout: true
         ) do
      {output, 0} ->
        lines = output |> String.split("\n", trim: true) |> Enum.map(&String.trim/1)

        eim_version =
          lines
          |> Enum.reverse()
          |> Enum.find(&String.match?(&1, ~r/^v?\d+(?:\.\d+)+$/))
          |> case do
            nil ->
              case Regex.run(~r/ESP_IDF_VERSION\s*=\s*([^\s]+)/, output, capture: :all_but_first) do
                [version] -> version
                _ -> "(unknown)"
              end

            version ->
              version
          end

        idf_dir =
          lines
          |> Enum.reverse()
          |> Enum.find(&String.starts_with?(&1, "/"))
          |> case do
            nil ->
              case Regex.run(~r/IDF_PATH\s*=\s*([^\s]+)/, output, capture: :all_but_first) do
                [path] -> path
                _ -> nil
              end

            path ->
              path
          end

        if not present?(idf_dir) do
          die("""
          Could not resolve IDF_PATH from EIM output.

          Output:
          #{String.trim(output)}
          """)
        end

        %{eim_version: eim_version, idf_dir: Path.expand(idf_dir)}

      {output, _status} ->
        die("""
        EIM is not ready.

        Make sure EIM is installed and an ESP-IDF version is selected first, for example:
          eim install -i #{@recommended_idf_version}
          eim select #{@recommended_idf_version}

        Output:
        #{String.trim(output)}
        """)
    end
  end


  defp ensure_supported_idf_version!(shared) do
    if supported_idf_version?(shared.eim_version) do
      :ok
    else
      die("""
      Unsupported ESP-IDF version selected in EIM: #{shared.eim_version}

      This script is currently tuned for AtomVM ESP32 builds with #{@recommended_idf_version}.x.

      Please switch EIM to #{@recommended_idf_version} and retry:
        eim install -i #{@recommended_idf_version}
        eim select #{@recommended_idf_version}

      Current IDF_PATH:
        #{shared.idf_dir}
      """)
    end
  end

  defp supported_idf_version?(version) when is_binary(version) do
    normalized =
      version
      |> String.trim()
      |> String.trim_leading("v")

    String.starts_with?(normalized, "5.5")
  end

  defp supported_idf_version?(_version), do: false

  defp resolve_atomvm_root(_repo_root, override) when is_binary(override) and override != "" do
    override = Path.expand(override)

    cond do
      File.dir?(Path.join(override, @atomvm_esp32_rel_path)) ->
        override

      File.dir?(Path.join(override, Path.join("AtomVM", @atomvm_esp32_rel_path))) ->
        Path.join(override, "AtomVM")

      true ->
        die("Could not find AtomVM under --atomvm-repo: #{override}")
    end
  end

  defp resolve_atomvm_root(repo_root, _override) do
    normalized = Path.expand(repo_root)

    if String.match?(normalized, ~r|/src/platforms/esp32/components/[^/]+$|) do
      esp32_dir = Path.expand("../..", normalized)
      Path.expand("../../..", esp32_dir)
    else
      Path.join(System.user_home!(), @default_atomvm_rel_path)
    end
  end

  defp ensure_atomvm_layout!(atomvm_root, esp32_dir) do
    if not File.dir?(atomvm_root), do: die("AtomVM repo directory not found: #{atomvm_root}")
    if not File.dir?(esp32_dir), do: die("AtomVM ESP32 platform dir not found: #{esp32_dir}")
  end

  defp ensure_component_link!(repo_root, esp32_dir) do
    name = Path.basename(repo_root)
    want = Path.join([esp32_dir, @components_dirname, name])

    File.mkdir_p!(Path.join(esp32_dir, @components_dirname))

    cond do
      symlink?(want) ->
        want_real = canonical_path(want)
        repo_real = canonical_path(repo_root)

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

        case File.ln_s(repo_root, want) do
          :ok -> :ok
          {:error, reason} -> die("Failed to create symlink: #{want} (#{inspect(reason)})")
        end
    end
  end

  defp ensure_project_sdkconfig_defaults!(repo_root) do
    path = Path.join(repo_root, @project_sdkconfig_defaults_filename)

    content =
      [
        "# Generated by #{@script_name}",
        "# Project-specific ESP-IDF config overrides for AtomVM ESP32 builds"
      ] ++ @project_sdkconfig_defaults

    File.write!(path, Enum.join(content, "\n") <> "\n")
    say("✔ wrote sdkconfig overrides: #{path}")
    path
  end

  defp build_boot_avm_if_needed!(atomvm_root) do
    boot_avm = Path.join(atomvm_root, @boot_avm_rel_path)

    unless File.regular?(boot_avm) do
      require_cmd!("cmake")
      say("Generating boot AVM (Generic UNIX build)")
      build_dir = Path.join(atomvm_root, "build")
      File.mkdir_p!(build_dir)
      run!("cmake", ["-S", atomvm_root, "-B", build_dir], env: host_tool_env())
      run!("cmake", ["--build", build_dir], env: host_tool_env())
      ensure_regular_file!(boot_avm, "boot AVM")
    end
  end

  defp build_host_tree!(atomvm_root, host_build_dir) do
    require_cmd!("cmake")
    File.mkdir_p!(host_build_dir)
    run!("cmake", ["-S", atomvm_root, "-B", host_build_dir], env: host_tool_env())
    run!("cmake", ["--build", host_build_dir], env: host_tool_env())
  end

  defp build_platform_tree!(esp32_dir, target, platform_build_dir, sdkconfig_overrides, opts) do
    cmake_args =
      platform_cmake_args(target) ++
        if Keyword.get(opts, :release, false), do: ["-DATOMVM_RELEASE=on"], else: []

    sdkconfig_defaults = "sdkconfig.defaults;#{sdkconfig_overrides}"

    run_idf!(
      esp32_dir,
      ["-B", platform_build_dir] ++ cmake_args ++ ["set-target", target],
      %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults}
    )

    run_idf!(
      esp32_dir,
      ["-B", platform_build_dir, "build"],
      %{"SDKCONFIG_DEFAULTS" => sdkconfig_defaults}
    )
  end

  defp run_idf!(workdir, idf_args, env \\ %{}) do
    command =
      ["idf.py", "-C", shell_display(workdir) | Enum.map(idf_args, &shell_display/1)]
      |> Enum.join(" ")

    display = "eim run #{command}"
    IO.puts(colorize(:cyan, "+ #{display}", bold: true))

    case System.cmd("eim", ["run", command],
           stderr_to_stdout: true,
           into: IO.stream(:stdio, :line),
           env: Enum.into(env, %{})
         ) do
      {_result, 0} ->
        :ok

      {_result, status} ->
        die("""
        ESP-IDF command failed (exit #{status}):
          #{display}

        Tips:
          - Look above for the first "FAILED:" or "Error:" line; EIM's final error can be generic.
          - This script currently expects #{@recommended_idf_version}.x for AtomVM ESP32 builds.
          - Check the selected EIM version with: eim list
          - Switch if needed:
              eim install -i #{@recommended_idf_version}
              eim select #{@recommended_idf_version}
          - Retry after cleaning if the build dir is stale:
              #{@script_name} clean
        """)
    end
  end

  defp platform_cmake_args(target) do
    args = ["-DATOMVM_ELIXIR_SUPPORT=on"]

    if target == "esp32c3" do
      args ++
        [
          "-DAVM_MINIMAL_OPCODES=ON",
          "-DLGFX_PORT_ENABLE_JP_FONTS=OFF",
          "-DLGFX_PORT_ENABLE_TOUCH=OFF"
        ]
    else
      args
    end
  end

  defp run_mkimage!(esp32_dir, mkimage_script, mkimage_extra_args) do
    args =
      [sh_escape(mkimage_script)] ++
        Enum.map(mkimage_extra_args, &sh_escape/1)

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

  defp ensure_serial_port_ready!(port) do
    if not File.exists?(port), do: die("Serial port not found: #{port}")

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
    cond do
      present?(serial_port_lsof_output(port)) ->
        "Detected by lsof:\n" <> serial_port_lsof_output(port)

      present?(serial_port_fuser_output(port)) ->
        "Detected by fuser:\n" <> serial_port_fuser_output(port)

      true ->
        nil
    end
  end

  defp serial_port_lsof_output(port), do: maybe_tool_output("lsof", ["-n", "-w", port])
  defp serial_port_fuser_output(port), do: maybe_tool_output("fuser", [port])

  defp maybe_tool_output(cmd, args) do
    if System.find_executable(cmd) do
      case System.cmd(cmd, args, stderr_to_stdout: true) do
        {output, 0} -> String.trim(output)
        _ -> ""
      end
    else
      ""
    end
  end

  defp print_file_if_present(label, path) do
    if File.regular?(path) do
      say("- #{label}: #{path}")
      content = File.read!(path)
      IO.write(content)
      if not String.ends_with?(content, "\n"), do: IO.puts("")
    else
      say("- #{label}: missing (#{path})")
    end
  end

  defp script_dir, do: Path.dirname(@script_file)
  defp repo_root, do: Path.expand("..", script_dir())
  defp present?(value), do: is_binary(value) and String.trim(value) != ""
  defp sha40?(ref), do: String.match?(ref, ~r/^[0-9a-f]{40}$/)
  defp looks_like_version_tag?(ref), do: String.match?(ref, ~r/^v?\d+\.\d+(\.\d+)?/)
  defp yes_no(true), do: "yes"
  defp yes_no(false), do: "no"

  defp git_head(repo_dir) do
    case System.cmd("git", ["rev-parse", "HEAD"], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} -> String.trim(out)
      _ -> "(unknown)"
    end
  end

  defp git_dirty?(repo_dir) do
    case System.cmd("git", ["status", "--porcelain"], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} -> String.trim(out) != ""
      _ -> false
    end
  end

  defp git_tracked_dirty?(repo_dir) do
    case System.cmd("git", ["status", "--porcelain=v1", "--untracked-files=no"],
           cd: repo_dir,
           stderr_to_stdout: true
         ) do
      {out, 0} -> String.trim(out) != ""
      _ -> false
    end
  end

  defp git_try_rev_parse(repo_dir, rev) do
    case System.cmd("git", ["rev-parse", "--verify", rev], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} -> {:ok, String.trim(out)}
      _ -> :error
    end
  end

  defp git_rev_parse!(repo_dir, rev) do
    case System.cmd("git", ["rev-parse", "--verify", rev], cd: repo_dir, stderr_to_stdout: true) do
      {out, 0} -> String.trim(out)
      {out, status} -> die("git rev-parse failed (#{status}) for #{rev}:\n#{String.trim(out)}")
    end
  end

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
    peeled_tag = "#{tag}^{}"
    args = ["ls-remote", "--tags", @atomvm_git_url, "refs/tags/#{peeled_tag}", "refs/tags/#{tag}"]

    case System.cmd("git", args, stderr_to_stdout: true) do
      {out, 0} ->
        lines = String.split(out, "\n", trim: true)

        peeled =
          Enum.find_value(lines, fn line ->
            case String.split(line, "\t") do
              [sha, "refs/tags/" <> rest] ->
                if rest == peeled_tag, do: sha, else: nil

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
          present?(peeled) -> {:ok, peeled}
          present?(direct) -> {:ok, direct}
          true -> :error
        end

      _ ->
        :error
    end
  end

  defp require_cmd!(cmd) do
    if System.find_executable(cmd), do: :ok, else: die("Missing dependency: #{cmd}")
  end

  defp host_tool_env do
    %{"PATH" => host_tool_path()}
  end

  defp host_tool_path do
    current_path = System.get_env("PATH", "")

    ["/usr/local/bin", "/usr/bin", "/bin", current_path]
    |> Enum.reject(&(&1 == ""))
    |> Enum.join(":")
  end

  defp ensure_regular_file!(path, label) do
    if File.regular?(path), do: :ok, else: die("#{label} not found: #{path}")
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

  defp run!(cmd, args, opts \\ []) do
    display =
      Keyword.get(opts, :display) || Enum.join([cmd | Enum.map(args, &shell_display/1)], " ")

    IO.puts(colorize(:cyan, "+ #{display}", bold: true))

    system_opts =
      [stderr_to_stdout: true, into: IO.stream(:stdio, :line)]
      |> Keyword.merge(Keyword.drop(opts, [:display]))

    case System.cmd(cmd, args, system_opts) do
      {_result, 0} -> :ok
      {_result, status} -> die("Command failed (exit #{status}): #{display}")
    end
  end

  defp shell_display(arg) do
    arg = to_string(arg)

    if String.contains?(arg, [" ", "\t", "\n", "'", "\"", "$", "`", "\\"]) do
      sh_escape(arg)
    else
      arg
    end
  end

  defp sh_escape(value), do: "'" <> String.replace(to_string(value), "'", ~s('"'"')) <> "'"

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

  defp colorize(color, text, opts \\ [])
  defp colorize(_color, text, _opts) when not is_binary(text), do: IO.iodata_to_binary(text)

  defp colorize(color, text, opts) do
    if ansi_enabled?() do
      maybe_bold = if Keyword.get(opts, :bold, false), do: [IO.ANSI.bright()], else: []

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

  defp ansi_enabled?, do: IO.ANSI.enabled?() and is_nil(System.get_env("NO_COLOR"))

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

  defp die_unknown_ref!(ref) do
    die("""
    Could not resolve AtomVM ref: #{ref}

    Expected one of:
      - branch name (e.g. main)
      - tag name (e.g. v0.6.6)
      - full SHA (40 hex chars)
    """)
  end
end

Main.main(System.argv())
