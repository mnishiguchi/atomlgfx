#!/usr/bin/env elixir

defmodule Main do
  @script_file Path.expand(__ENV__.file)
  @script_name Path.basename(@script_file)

  @default_target "esp32s3"
  @default_idf_rel_path "esp/esp-idf"
  @default_atomvm_rel_path "atomvm/AtomVM"

  @atomvm_esp32_rel_path "src/platforms/esp32"
  @idf_export_sh_filename "export.sh"

  @default_host_build_dirname "build"
  @default_platform_build_rel_path Path.join(@atomvm_esp32_rel_path, "build")

  @preferred_boot_avm_rel_path "build/libs/esp32boot/elixir_esp32boot.avm"
  @fallback_boot_avm_rel_path "build/libs/esp32boot/esp32boot.avm"

  def main(argv) do
    {options, passthrough_args, invalid} =
      OptionParser.parse(argv,
        strict: [
          atomvm_repo: :string,
          idf_dir: :string,
          target: :string,
          host_build_dir: :string,
          platform_build_dir: :string,
          boot: :string,
          skip_host_build: :boolean,
          skip_platform_build: :boolean,
          help: :boolean
        ],
        aliases: [h: :help]
      )

    cond do
      invalid != [] ->
        {flag, _value} = List.first(invalid)
        die("Unknown option: #{flag} (use --help)")

      Keyword.get(options, :help, false) ->
        usage()
        System.halt(0)

      true ->
        run(options, passthrough_args)
    end
  end

  defp run(options, mkimage_extra_args) do
    atomvm_repo_override = Keyword.get(options, :atomvm_repo, "")
    idf_dir_override = Keyword.get(options, :idf_dir, "")
    target = Keyword.get(options, :target, @default_target)
    host_build_dir_override = Keyword.get(options, :host_build_dir, "")
    platform_build_dir_override = Keyword.get(options, :platform_build_dir, "")
    boot_override = Keyword.get(options, :boot, "")
    skip_host_build = Keyword.get(options, :skip_host_build, false)
    skip_platform_build = Keyword.get(options, :skip_platform_build, false)

    this_repo_root = repo_root()
    idf_dir = resolve_idf_dir(idf_dir_override)
    {atomvm_root, esp32_dir} = resolve_atomvm_paths(this_repo_root, atomvm_repo_override)

    host_build_dir =
      if present?(host_build_dir_override) do
        Path.expand(host_build_dir_override)
      else
        Path.join(atomvm_root, @default_host_build_dirname)
      end

    platform_build_dir =
      if present?(platform_build_dir_override) do
        Path.expand(platform_build_dir_override)
      else
        Path.join(atomvm_root, @default_platform_build_rel_path)
      end

    boot_avm_path =
      cond do
        present?(boot_override) ->
          Path.expand(boot_override)

        true ->
          infer_boot_avm_path(atomvm_root)
      end

    mkimage_script = Path.join(platform_build_dir, "mkimage.sh")

    ensure_atomvm_layout!(atomvm_root, esp32_dir)
    ensure_regular_file!(Path.join(idf_dir, @idf_export_sh_filename), "ESP-IDF export.sh")

    if not skip_host_build do
      require_cmd!("cmake")
      say("Building AtomVM host tree")
      build_host_tree!(atomvm_root, host_build_dir)
    else
      say("Skipping AtomVM host build")
    end

    if not skip_platform_build do
      say("Building AtomVM ESP32 platform")
      build_platform_tree!(idf_dir, esp32_dir, target)
    else
      say("Skipping AtomVM ESP32 platform build")
    end

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

  defp usage do
    IO.puts("""
    Usage:
      #{@script_name} [options] [mkimage args...]

    Purpose:
      Build a custom AtomVM release image for this lgfx port driver project.

    Options:
      --atomvm-repo PATH       AtomVM repo root (or wrapper containing AtomVM/)
                              default: $HOME/#{@default_atomvm_rel_path}

      --idf-dir PATH           ESP-IDF root (contains export.sh). Optional.
                              Uses ESP_IDF_DIR, then IDF_PATH, else:
                              $HOME/#{@default_idf_rel_path}

      --target TARGET          ESP target for idf.py set-target
                              default: #{@default_target}

      --host-build-dir PATH    AtomVM host build directory
                              default: <atomvm_repo>/#{@default_host_build_dirname}

      --platform-build-dir PATH
                              AtomVM ESP32 platform build directory
                              default: <atomvm_repo>/#{@default_platform_build_rel_path}

      --boot PATH              Explicit boot AVM path
                              default: infer:
                                1. #{@preferred_boot_avm_rel_path}
                                2. #{@fallback_boot_avm_rel_path}

      --skip-host-build        Skip cmake host build
      --skip-platform-build    Skip idf.py platform build
      -h, --help               Show help

    Notes:
      - Extra positional arguments are forwarded to mkimage.sh unchanged.
      - This script builds the release image only. It does not flash it.

    Examples:
      #{@script_name}
      #{@script_name} --target esp32s3
      #{@script_name} -- --main build/my_app.avm
      #{@script_name} -- --main build/my_app.avm --data build/assets.avm
      #{@script_name} --skip-host-build --skip-platform-build -- --help
    """)
  end

  defp repo_root do
    @script_file
    |> Path.dirname()
    |> Path.expand("..")
  end

  defp resolve_idf_dir(""), do: resolve_idf_dir(nil)

  defp resolve_idf_dir(nil) do
    cond do
      present?(System.get_env("ESP_IDF_DIR")) -> Path.expand(System.get_env("ESP_IDF_DIR"))
      present?(System.get_env("IDF_PATH")) -> Path.expand(System.get_env("IDF_PATH"))
      true -> Path.join(System.user_home!(), @default_idf_rel_path)
    end
  end

  defp resolve_idf_dir(path), do: Path.expand(path)

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

  defp ensure_atomvm_layout!(atomvm_root, esp32_dir) do
    if not File.dir?(atomvm_root) do
      die("AtomVM repo directory not found: #{atomvm_root}")
    end

    if not File.dir?(esp32_dir) do
      die("AtomVM ESP32 platform dir not found: #{esp32_dir}")
    end
  end

  defp infer_boot_avm_path(atomvm_root) do
    preferred = Path.join(atomvm_root, @preferred_boot_avm_rel_path)
    fallback = Path.join(atomvm_root, @fallback_boot_avm_rel_path)

    cond do
      File.regular?(preferred) -> preferred
      File.regular?(fallback) -> fallback
      true -> preferred
    end
  end

  defp build_host_tree!(atomvm_root, host_build_dir) do
    File.mkdir_p!(host_build_dir)
    run!("cmake", ["-S", atomvm_root, "-B", host_build_dir])
    run!("cmake", ["--build", host_build_dir])
  end

  defp build_platform_tree!(idf_dir, esp32_dir, target) do
    with_idf_env!(
      idf_dir,
      esp32_dir,
      """
      echo "+ idf.py set-target #{sh_escape(target)}"
      idf.py set-target #{sh_escape(target)}

      echo "+ idf.py build"
      idf.py build
      """
    )
  end

  defp run_mkimage!(esp32_dir, mkimage_script, host_build_dir, boot_avm_path, mkimage_extra_args) do
    args = [
      sh_escape(mkimage_script),
      "--build_dir",
      sh_escape(host_build_dir),
      "--boot",
      sh_escape(boot_avm_path)
      | Enum.map(mkimage_extra_args, &sh_escape/1)
    ]

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

  defp with_idf_env!(idf_dir, workdir, shell_body) do
    export_sh = Path.join(idf_dir, @idf_export_sh_filename)

    ensure_regular_file!(export_sh, "ESP-IDF export.sh")

    if not File.dir?(workdir) do
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

  defp die(message) do
    IO.puts(:stderr, colorize(:red, "✖ #{message}", bold: true))
    System.halt(1)
  end

  defp say(message) do
    cond do
      String.starts_with?(message, "✔") -> IO.puts(colorize(:green, message))
      true -> IO.puts(message)
    end
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
