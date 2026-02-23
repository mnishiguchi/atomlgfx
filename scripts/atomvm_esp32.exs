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

  @atomvm_git_url "https://github.com/atomvm/AtomVM.git"
  @atomvm_git_branch "main"

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

          extra_args != [] ->
            die("Unexpected positional arguments: #{Enum.join(extra_args, " ")}")

          Keyword.get(options, :help, false) ->
            usage()
            System.halt(0)

          true ->
            run_command(command, options)
        end
    end
  end

  defp run_command(command, options) do
    this_repo_root = repo_root()

    atomvm_repo_override = Keyword.get(options, :atomvm_repo, "")
    idf_dir_override = Keyword.get(options, :idf_dir, "")
    target = Keyword.get(options, :target, @default_target)
    port = Keyword.get(options, :port, "")
    override_was_set = atomvm_repo_override != ""

    idf_dir = resolve_idf_dir(idf_dir_override)
    {atomvm_root, esp32_dir} = resolve_atomvm_paths(this_repo_root, atomvm_repo_override)
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
          override_was_set
        )

      "install" ->
        install_cmd(
          this_repo_root,
          atomvm_root,
          esp32_dir,
          idf_dir,
          target,
          port,
          override_was_set
        )

      "monitor" ->
        monitor_cmd(esp32_dir, idf_dir, port)

      _ ->
        usage()
        die("Unknown command: #{command}")
    end
  end

  defp usage do
    IO.puts("""
    Usage:
      #{@script_name} <command> [options]

    Commands:
      info      Print resolved paths and basic checks (no changes)
      install   Ensure AtomVM exists, link component, patch config, build + flash firmware
      monitor   Attach serial monitor (idf.py monitor)

    Options:
      --atomvm-repo PATH   AtomVM repo root (or wrapper containing AtomVM/)
      --idf-dir PATH       ESP-IDF root (contains export.sh). Optional.
      --target TARGET      esp32 / esp32s3 / etc (default: #{@default_target})
      --port PORT          Serial device (required for install/monitor)
      -h, --help           Show help

    Examples:
      #{@script_name} info
      #{@script_name} install --target esp32s3 --port /dev/ttyACM0
      #{@script_name} monitor --port /dev/ttyACM0

    ESP-IDF discovery (if --idf-dir not provided):
      Uses ESP_IDF_DIR, then IDF_PATH, else defaults to: $HOME/#{@default_idf_rel_path}
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

  defp ensure_atomvm_repo!(atomvm_root, override_was_set) do
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
          @atomvm_git_branch,
          @atomvm_git_url,
          atomvm_root
        ])
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

  # Keep ESP-IDF/Python env isolated in a non-login Bash subshell.
  defp with_idf_env!(idf_dir, workdir, shell_body) do
    export_sh = Path.join(idf_dir, @idf_export_sh_filename)

    if !File.regular?(export_sh) do
      die("ESP-IDF export.sh not found: #{export_sh}")
    end

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
         override_was_set
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

    say("")
    say("Checks")

    if File.regular?(Path.join(idf_dir, @idf_export_sh_filename)) do
      say("- ESP-IDF:     export.sh found")
    else
      say("- ESP-IDF:     missing export.sh")
    end

    if File.dir?(Path.join(atomvm_root, ".git")) do
      say("- AtomVM:      ok")
    else
      if override_was_set do
        say("- AtomVM:      missing at --atomvm-repo")
      else
        say("- AtomVM:      missing at default (install will clone)")
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
         override_was_set
       ) do
    if port == "" do
      die("--port is required for install (e.g. --port /dev/ttyACM0)")
    end

    ensure_serial_port_ready!(port)

    ensure_atomvm_repo!(atomvm_root, override_was_set)

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

    # Another process may grab the port while the build is running.
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

      # Ensure boot.avm partition is written (idf.py flash does not always include data partitions).
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
end

Main.main(System.argv())
