#!/usr/bin/env elixir

# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule Main do
  @script_file Path.expand(__ENV__.file)
  @script_name Path.basename(@script_file)
  @component_root Path.expand("..", Path.dirname(@script_file))

  @default_target "esp32s3"
  @recommended_idf_version "v5.5"
  @default_atomvm_rel_path "atomvm/AtomVM"

  @atomvm_esp32_rel_path "src/platforms/esp32"
  @components_dirname "components"
  @sdkconfig_defaults_filename "sdkconfig.defaults"
  @sdkconfig_defaults_dirname "config"

  @elixir_boot_avm_rel_path "build/libs/esp32boot/elixir_esp32boot.avm"
  @erlang_boot_avm_rel_path "build/libs/esp32boot/esp32boot.avm"
  @default_host_build_dirname "build"
  @default_platform_build_rel_path Path.join(@atomvm_esp32_rel_path, "build")

  @atomvm_git_url "https://github.com/atomvm/AtomVM.git"
  @atomvm_clone_branch "release-0.7"
  # Keep release builds reproducible even while the upstream release branch moves.
  @atomvm_default_ref "e2cacc998f455ad66b1aa9e6391f7b32928cc38d"

  # ESP-IDF disables Xtensa hardware atomics and provides a software-backed
  # stdatomic implementation. AtomVM's current probe rejects that supported
  # configuration because the compiler reports it as not always lock-free.
  @atomvm_idf_atomic_probe_override "-DATOMIC_POINTER_LOCK_FREE_IS_TWO=ON"
  @atomvm_dual_core_xtensa_targets ["esp32", "esp32s3"]

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
              atomvm_dir: :string,
              atomvm_repo: :string,
              atomvm_ref: :string,
              idf_version: :string,
              allow_dirty: :boolean,
              target: :string,
              port: :string,
              component: :string,
              sdkconfig: :string,
              cmake_define: :string,
              out_dir: :string,
              elixir_support: :boolean,
              release: :boolean,
              fullclean: :boolean,
              clean_host: :boolean,
              clean_platform: :boolean,
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
    atomvm_root = resolve_atomvm_root(options)
    esp32_dir = Path.join(atomvm_root, @atomvm_esp32_rel_path)
    target = Keyword.get(options, :target, @default_target)
    port = Keyword.get(options, :port, "")
    port_display = if port == "", do: "(not set)", else: port
    allow_dirty = Keyword.get(options, :allow_dirty, false) or truthy_env?("ATOMVM_ALLOW_DIRTY")
    expected_idf_version = resolve_expected_idf_version(options)
    elixir_support = Keyword.get(options, :elixir_support, true)
    release = Keyword.get(options, :release, false)
    fullclean = Keyword.get(options, :fullclean, false)
    clean_host = Keyword.get(options, :clean_host, false)
    clean_platform = Keyword.get(options, :clean_platform, false)
    requested_components = Keyword.get_values(options, :component) |> Enum.map(&Path.expand/1)
    sdkconfig_components =
      uniq_paths([@component_root | requested_components ++ linked_components(esp32_dir)])
    explicit_sdkconfigs = Keyword.get_values(options, :sdkconfig) |> Enum.map(&Path.expand/1)

    sdkconfigs =
      uniq_paths(
        component_sdkconfigs(sdkconfig_components, target) ++
          local_sdkconfigs(target) ++ explicit_sdkconfigs
      )

    cmake_defines = Keyword.get_values(options, :cmake_define)
    out_dir = options |> Keyword.get(:out_dir, "") |> expand_optional_path()
    {atomvm_ref, atomvm_ref_source} = resolve_atomvm_ref(options)

    shared = %{
      atomvm_root: atomvm_root,
      esp32_dir: esp32_dir,
      target: target,
      port: port,
      port_display: port_display,
      atomvm_ref: atomvm_ref,
      atomvm_ref_source: atomvm_ref_source,
      allow_dirty: allow_dirty,
      expected_idf_version: expected_idf_version,
      elixir_support: elixir_support,
      release: release,
      fullclean: fullclean,
      clean_host: clean_host,
      clean_platform: clean_platform,
      components: requested_components,
      sdkconfig_components: sdkconfig_components,
      sdkconfigs: sdkconfigs,
      cmake_defines: cmake_defines,
      out_dir: out_dir
    }

    case command do
      "clean" ->
        clean_platform_cmd(shared)

      "clean-platform" ->
        clean_platform_cmd(shared)

      "clean-host" ->
        clean_host_cmd(shared)

      "fetch" ->
        fetch_cmd(shared)

      "build-host" ->
        build_host_cmd(shared)

      "component-status" ->
        component_status_cmd(shared)

      "component-link" ->
        component_link_cmd(shared)

      "component-unlink" ->
        component_unlink_cmd(shared)

      _ ->
        eim_info = resolve_eim_env_info!()
        shared = Map.merge(shared, eim_info)

        if command in ["install", "build", "flash", "mkimage", "monitor"] do
          ensure_supported_idf_version!(shared)
        end

        case command do
          "info" ->
            info_cmd(shared)

          "build" ->
            build_cmd(shared)

          "flash" ->
            flash_cmd(shared)

          "install" ->
            install_cmd(shared)

          "monitor" ->
            monitor_cmd(shared)

          "mkimage" ->
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
      info        Print resolved paths and build settings
      fetch       Ensure AtomVM exists and pin it to the selected ref
      build-host  Build AtomVM host tools only
      build       Build AtomVM ESP32 firmware
      flash       Flash already-built AtomVM ESP32 firmware
      install     Fetch, build, and flash AtomVM ESP32 firmware
      monitor     Attach serial monitor (idf.py monitor via EIM)
      mkimage     Build a custom AtomVM ESP32 release image
      clean       Remove AtomVM ESP32 platform build directory
      clean-platform
                  Remove AtomVM ESP32 platform build directory
      clean-host  Remove AtomVM host build directory
      component-status
                  Show the AtomVM ESP32 component symlink status
      component-link
                  Link a component into AtomVM ESP32 components
      component-unlink
                  Remove the component symlink when it points to the requested component

    Common options:
      --atomvm-dir PATH       AtomVM repo root
                              default: ~/#{@default_atomvm_rel_path}
                              env: ATOMVM_DIR
      --atomvm-repo PATH      Backward-compatible alias for --atomvm-dir
      --atomvm-ref REF        AtomVM ref: branch/tag/full SHA
                              default: #{@atomvm_default_ref}
                              env: ATOMVM_REF
      --idf-version VERSION   Expected ESP-IDF version prefix
                              default: #{@recommended_idf_version}
                              env: ATOMVM_IDF_VERSION
      --allow-dirty           Allow pinning even if AtomVM repo has tracked local changes
                              env: ATOMVM_ALLOW_DIRTY=1
      --target TARGET         esp32 / esp32s3 / esp32c3 / etc (default: #{@default_target})
      --port PORT             Serial device (required for install/flash/monitor)
      --no-elixir-support     Build without AtomVM Elixir support
      --release               Add -DATOMVM_RELEASE=on for normal build/install
      --fullclean             Run idf.py fullclean before build
      --clean-platform        Remove AtomVM ESP32 platform build dir before building
      --clean-host            Remove AtomVM host build dir before building host tools
      -h, --help              Show help

    Optional integration options:
      --component PATH        Link an external ESP-IDF component into AtomVM before building
                              May be passed multiple times
      --sdkconfig PATH        Append an extra SDKCONFIG_DEFAULTS file
                              May be passed multiple times
      --cmake-define KEY=VAL  Add a CMake -D option
                              May be passed multiple times

    mkimage options:
      --out-dir PATH          Copy the generated .img file to this directory
                              default: AtomVM ESP32 platform build directory

    Workflow:
      - This script assumes ESP-IDF is managed by EIM.
      - It is currently tuned for AtomVM ESP32 builds with #{@recommended_idf_version}.x.
      - Install/select the recommended version first:
          eim install -i #{@recommended_idf_version}
          eim select #{@recommended_idf_version}
      - The script runs idf.py through EIM automatically.
      - No activation script sourcing is required.

    Generic defaults:
      - No external project component is linked unless --component is passed.
      - Linked/current component SDK defaults are used when present.
      - Supported paths: sdkconfig.defaults, sdkconfig.defaults.<target>,
        config/sdkconfig.defaults, config/sdkconfig.defaults.<target>
      - Elixir support is enabled by default. Use --no-elixir-support to opt out.
      - The default AtomVM ref is an exact release-0.7 commit for reproducible builds.
      - ESP32/ESP32-S3 builds keep SMP enabled using ESP-IDF's stdatomic implementation.
      - The bundled scheduler-stack sdkconfig default is included from the component root.

    Examples:
      eim install -i #{@recommended_idf_version}
      eim select #{@recommended_idf_version}
      #{@script_name} info
      #{@script_name} fetch
      #{@script_name} build --target esp32s3
      #{@script_name} install --target esp32s3 --port /dev/ttyACM0
      #{@script_name} install --target esp32s3 --port /dev/ttyACM0 --no-elixir-support
      #{@script_name} build --component . --sdkconfig ./sdkconfig.defaults.extra
      #{@script_name} build --cmake-define AVM_MINIMAL_OPCODES=ON
      #{@script_name} flash --target esp32s3 --port /dev/ttyACM0
      #{@script_name} monitor --port /dev/ttyACM0
      #{@script_name} mkimage --target esp32s3
      #{@script_name} mkimage --target esp32s3 --out-dir ./build/atomvm-images
      #{@script_name} mkimage --clean-host --clean-platform --target esp32c3 --out-dir ~/Desktop
      #{@script_name} mkimage -- --main build/my_app.avm
      #{@script_name} component-status --component .
      #{@script_name} component-link --component .
      #{@script_name} component-unlink --component .
      #{@script_name} clean
      #{@script_name} clean-host

    AtomVM pinning:
      - Branch ref (e.g. main): always fetches origin/<branch> with depth=1 before resolving
      - Tag ref (e.g. v0.6.6): resolves tag target commit
      - SHA ref (40 hex): checks out that commit
    """)
  end

  defp info_cmd(shared) do
    sdkconfig_defaults = Path.join(shared.esp32_dir, @sdkconfig_defaults_filename)
    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)
    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)

    say("")
    say("Paths")
    say("- atomvm_root:        #{shared.atomvm_root}")
    say("- esp32_dir:          #{shared.esp32_dir}")
    say("- host_build_dir:     #{host_build_dir}")
    say("- platform_build_dir: #{platform_build_dir}")
    say("- idf_dir:            #{shared.idf_dir}")

    say("")
    say("Config")
    say("- target:             #{shared.target}")
    say("- port:               #{shared.port_display}")
    say("- atomvm_ref:         #{shared.atomvm_ref}")
    say("- ref_source:         #{shared.atomvm_ref_source}")
    say("- allow_dirty:        #{shared.allow_dirty}")
    say("- eim_version:        #{shared.eim_version}")
    say("- expected_idf:       #{shared.expected_idf_version}.x")

    say(
      "- idf_ok:             #{if supported_idf_version?(shared.eim_version, shared.expected_idf_version), do: "yes", else: "no"}"
    )

    say("- elixir_support:     #{shared.elixir_support}")
    say("- release:            #{shared.release}")
    say("- fullclean:          #{shared.fullclean}")
    say("- clean_host:         #{shared.clean_host}")
    say("- clean_platform:     #{shared.clean_platform}")

    say("")
    say("Optional integration")
    say("- requested components: #{display_list(shared.components)}")
    say("- sdkconfig components: #{display_list(shared.sdkconfig_components)}")
    say("- sdkconfigs:           #{display_list(shared.sdkconfigs)}")
    say("- cmake_defines:      #{display_list(shared.cmake_defines)}")
    say("- compatibility defs: #{display_list(platform_compatibility_cmake_args(shared))}")

    say(
      "- mkimage_out_dir:    #{if present?(shared.out_dir), do: shared.out_dir, else: "(platform build dir)"}"
    )

    say("")
    say("Checks")
    say("- EIM:                ok")
    say("- ESP-IDF dir:        #{if File.dir?(shared.idf_dir), do: "ok", else: "missing"}")

    if File.dir?(Path.join(shared.atomvm_root, ".git")) do
      say("- AtomVM:             ok")
      say("- AtomVM HEAD:        #{git_head(shared.atomvm_root)}")
      say("- AtomVM dirty(tracked): #{yes_no(git_tracked_dirty?(shared.atomvm_root))}")
      say("- AtomVM dirty(any):     #{yes_no(git_dirty?(shared.atomvm_root))}")
    else
      say("- AtomVM:             missing (fetch/install/build will clone)")

      case maybe_resolve_atomvm_ref_without_repo(shared.atomvm_ref) do
        {:ok, sha, note} when is_binary(sha) and sha != "" ->
          say("- AtomVM would use:  #{sha}  (#{note})")

        {:ok, nil, note} ->
          say("- AtomVM would use:  (not resolved)  (#{note})")

        {:error, reason} ->
          say("- AtomVM would use:  (not resolved)  (#{reason})")
      end
    end

    say("- ESP32 dir:          #{if File.dir?(shared.esp32_dir), do: "ok", else: "missing"}")

    say(
      "- sdkconfig.defaults: #{if File.regular?(sdkconfig_defaults), do: "ok", else: "missing"}"
    )

    components_dir = Path.join(shared.esp32_dir, @components_dirname)

    say("")
    say("Inspect")

    say(
      "- components dir:     #{if File.dir?(components_dir), do: components_dir, else: "missing (#{components_dir})"}"
    )

    if File.dir?(components_dir) do
      case File.ls(components_dir) do
        {:ok, entries} -> entries |> Enum.sort() |> Enum.each(&IO.puts/1)
        {:error, reason} -> say("- failed to list components: #{inspect(reason)}")
      end
    end

    say("")
  end

  defp fetch_cmd(shared) do
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)
    say("✔ fetch complete")
  end

  defp build_host_cmd(shared) do
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)
    maybe_clean_host_build_dir!(shared, host_build_dir)
    say("Building AtomVM host tree")
    build_host_tree!(shared.atomvm_root, host_build_dir)
    say("✔ host build complete")
  end

  defp build_cmd(shared) do
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)
    ensure_optional_integration!(shared)

    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)
    maybe_clean_host_build_dir!(shared, host_build_dir)
    build_boot_avm_if_needed!(shared)

    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)

    say("Building AtomVM ESP32 firmware")

    prepare_platform_build_dir!(shared, platform_build_dir)

    build_platform_tree!(shared, platform_build_dir, release: shared.release)
    say("✔ build complete")
  end

  defp flash_cmd(shared) do
    if shared.port == "", do: die("--port is required for flash (e.g. --port /dev/ttyACM0)")

    ensure_serial_port_ready!(shared.port)
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)

    say("Flashing AtomVM ESP32 firmware")

    run_idf!(
      shared.esp32_dir,
      ["-B", platform_build_dir, "-p", shared.port, "flash"],
      sdkconfig_env(shared)
    )

    say("✔ flash complete")
  end

  defp install_cmd(shared) do
    if shared.port == "", do: die("--port is required for install (e.g. --port /dev/ttyACM0)")

    build_cmd(shared)
    flash_cmd(shared)
    say("✔ install complete")
  end

  defp monitor_cmd(shared) do
    if shared.port == "", do: die("--port is required for monitor (e.g. --port /dev/ttyACM0)")

    ensure_serial_port_ready!(shared.port)
    say("Starting serial monitor")
    run_idf!(shared.esp32_dir, ["-p", shared.port, "monitor"])
  end

  defp mkimage_cmd(shared, mkimage_extra_args) do
    ensure_atomvm_repo!(shared)
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)
    ensure_optional_integration!(shared)

    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)
    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)
    boot_avm_path = Path.join(shared.atomvm_root, boot_avm_rel_path(shared))
    mkimage_script = Path.join(platform_build_dir, "mkimage.sh")

    maybe_clean_host_build_dir!(shared, host_build_dir)
    prepare_platform_build_dir!(shared, platform_build_dir)

    say("Building AtomVM host tree")
    build_host_tree!(shared.atomvm_root, host_build_dir)

    say("Building AtomVM ESP32 platform for release image")
    build_platform_tree!(shared, platform_build_dir, release: true)

    ensure_regular_file!(boot_avm_path, "boot AVM")
    ensure_regular_file!(mkimage_script, "mkimage.sh")

    say("Creating release image")
    run_mkimage!(shared.esp32_dir, mkimage_script, mkimage_extra_args)

    case newest_image_path(platform_build_dir) do
      nil ->
        die("mkimage.sh finished, but no .img file was found in #{platform_build_dir}")

      image_path ->
        output_path = maybe_copy_image_to_out_dir!(image_path, shared.out_dir)
        say("✔ release image ready")
        IO.puts(output_path)
    end
  end

  defp clean_platform_cmd(shared) do
    platform_build_dir = Path.join(shared.atomvm_root, @default_platform_build_rel_path)
    remove_dir_if_present!(platform_build_dir, "AtomVM ESP32 platform build dir")
    say("✔ clean complete")
  end

  defp clean_host_cmd(shared) do
    host_build_dir = Path.join(shared.atomvm_root, @default_host_build_dirname)
    remove_dir_if_present!(host_build_dir, "AtomVM host build dir")
    say("✔ clean-host complete")
  end

  defp component_status_cmd(shared) do
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    shared
    |> component_command_components()
    |> Enum.each(&show_component_status!(&1, shared.esp32_dir))
  end

  defp component_link_cmd(shared) do
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    shared
    |> component_command_components()
    |> Enum.each(&ensure_component_link!(&1, shared.esp32_dir))

    say("✔ component-link complete")
  end

  defp component_unlink_cmd(shared) do
    ensure_atomvm_layout!(shared.atomvm_root, shared.esp32_dir)

    shared
    |> component_command_components()
    |> Enum.each(&remove_component_link!(&1, shared.esp32_dir))

    say("✔ component-unlink complete")
  end

  defp component_command_components(shared) do
    case shared.components do
      [] -> [File.cwd!()]
      components -> components
    end
  end

  defp prepare_platform_build_dir!(shared, platform_build_dir) do
    cond do
      shared.clean_platform ->
        remove_dir_if_present!(platform_build_dir, "AtomVM ESP32 platform build dir")

      shared.fullclean ->
        run_idf!(shared.esp32_dir, ["-B", platform_build_dir, "fullclean"], sdkconfig_env(shared))

      true ->
        :ok
    end
  end

  defp maybe_clean_host_build_dir!(shared, host_build_dir) do
    if shared.clean_host do
      remove_dir_if_present!(host_build_dir, "AtomVM host build dir")
    end
  end

  defp remove_dir_if_present!(path, label) do
    if File.dir?(path) do
      say("Removing #{label}: #{path}")
      File.rm_rf!(path)
    else
      say("#{label} not present: #{path}")
    end
  end

  defp ensure_optional_integration!(shared) do
    Enum.each(shared.components, &ensure_component_link!(&1, shared.esp32_dir))
    Enum.each(shared.sdkconfigs, &ensure_regular_file!(&1, "SDKCONFIG_DEFAULTS file"))
  end

  defp ensure_atomvm_repo!(shared) do
    ensure_atomvm_repo!(
      shared.atomvm_root,
      shared.atomvm_ref,
      shared.atomvm_ref_source,
      shared.allow_dirty
    )
  end

  defp ensure_atomvm_repo!(atomvm_root, atomvm_ref, atomvm_ref_source, allow_dirty) do
    cond do
      File.dir?(Path.join(atomvm_root, ".git")) ->
        say("✔ AtomVM repo exists: #{atomvm_root}")

      File.exists?(atomvm_root) ->
        die("AtomVM path exists but is not a git repo: #{atomvm_root}")

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

          To bypass this check:
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
    case fetch_name_ref_commit(repo_dir, ref) do
      {:ok, commit} ->
        commit

      :error ->
        case git_try_rev_parse(repo_dir, "#{ref}^{commit}") do
          {:ok, commit} -> commit
          :error -> die_unknown_ref!(repo_dir, ref)
        end
    end
  end

  defp fetch_name_ref_commit(repo_dir, ref) do
    if looks_like_version_tag?(ref) do
      case fetch_tag_commit(repo_dir, ref) do
        {:ok, commit} -> {:ok, commit}
        :error -> fetch_branch_commit(repo_dir, ref)
      end
    else
      case fetch_branch_commit(repo_dir, ref) do
        {:ok, commit} -> {:ok, commit}
        :error -> fetch_tag_commit(repo_dir, ref)
      end
    end
  end

  defp fetch_tag_commit(repo_dir, tag) do
    fetch_ref_commit(repo_dir, "refs/tags/#{tag}")
  end

  defp fetch_branch_commit(repo_dir, branch) do
    fetch_ref_commit(repo_dir, "refs/heads/#{branch}")
  end

  defp fetch_ref_commit(repo_dir, source_ref) do
    ["origin", @atomvm_git_url]
    |> Enum.find_value(:error, fn remote_or_url ->
      case System.cmd("git", ["fetch", "--depth", "1", remote_or_url, source_ref],
             cd: repo_dir,
             stderr_to_stdout: true
           ) do
        {_, 0} -> {:ok, git_rev_parse!(repo_dir, "FETCH_HEAD^{commit}")}
        _ -> nil
      end
    end)
  end

  defp resolve_atomvm_ref(options) do
    cli = options |> Keyword.get(:atomvm_ref, "") |> to_string() |> String.trim()
    env = System.get_env("ATOMVM_REF", "") |> String.trim()

    cond do
      present?(cli) -> {cli, "--atomvm-ref"}
      present?(env) -> {env, "ATOMVM_REF"}
      true -> {@atomvm_default_ref, "default(#{@atomvm_default_ref})"}
    end
  end

  defp resolve_atomvm_root(options) do
    atomvm_dir =
      options
      |> Keyword.get(:atomvm_dir, "")
      |> first_present(Keyword.get(options, :atomvm_repo, ""))
      |> first_present(System.get_env("ATOMVM_DIR", ""))

    if present?(atomvm_dir) do
      Path.expand(atomvm_dir)
    else
      Path.join(System.user_home!(), @default_atomvm_rel_path)
    end
  end

  defp resolve_expected_idf_version(options) do
    cli = options |> Keyword.get(:idf_version, "") |> to_string() |> String.trim()
    env = System.get_env("ATOMVM_IDF_VERSION", "") |> String.trim()

    cond do
      present?(cli) -> cli
      present?(env) -> env
      true -> @recommended_idf_version
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

        eim_version =
          if present?(eim_version) do
            eim_version
          else
            case Regex.run(~r/ESP_IDF_VERSION\s*=\s*([^\s]+)/, output, capture: :all_but_first) do
              [version] -> version
              _ -> "(unknown)"
            end
          end

        idf_dir =
          lines
          |> Enum.reverse()
          |> Enum.find(&String.starts_with?(&1, "/"))

        idf_dir =
          if present?(idf_dir) do
            idf_dir
          else
            case Regex.run(~r/IDF_PATH\s*=\s*([^\s]+)/, output, capture: :all_but_first) do
              [path] -> path
              _ -> nil
            end
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
    if supported_idf_version?(shared.eim_version, shared.expected_idf_version) do
      :ok
    else
      die("""
      Unsupported ESP-IDF version selected in EIM: #{shared.eim_version}

      This script currently expects AtomVM ESP32 builds with #{shared.expected_idf_version}.x.

      Please switch EIM and retry:
        eim install -i #{shared.expected_idf_version}
        eim select #{shared.expected_idf_version}

      Current IDF_PATH:
        #{shared.idf_dir}
      """)
    end
  end

  defp supported_idf_version?(version, expected)
       when is_binary(version) and is_binary(expected) do
    normalized_version = version |> String.trim() |> String.trim_leading("v")
    normalized_expected = expected |> String.trim() |> String.trim_leading("v")

    String.starts_with?(normalized_version, normalized_expected)
  end

  defp supported_idf_version?(_version, _expected), do: false

  defp ensure_atomvm_layout!(atomvm_root, esp32_dir) do
    if not File.dir?(atomvm_root), do: die("AtomVM repo directory not found: #{atomvm_root}")
    if not File.dir?(esp32_dir), do: die("AtomVM ESP32 platform dir not found: #{esp32_dir}")
  end

  defp ensure_component_link!(component_dir, esp32_dir) do
    if not File.dir?(component_dir), do: die("Component directory not found: #{component_dir}")

    name = Path.basename(component_dir)
    want = Path.join([esp32_dir, @components_dirname, name])

    File.mkdir_p!(Path.join(esp32_dir, @components_dirname))

    cond do
      symlink?(want) ->
        want_real = canonical_path(want)
        component_real = canonical_path(component_dir)

        cond do
          present?(want_real) and present?(component_real) and want_real == component_real ->
            say("✔ component symlink ok: #{want}")

          present?(want_real) and present?(component_real) ->
            die("Component symlink exists but points elsewhere: #{want} -> #{want_real}")

          true ->
            say("✔ component symlink present: #{want}")
        end

      File.exists?(want) ->
        die("Component path exists but is not a symlink: #{want}")

      true ->
        say("Linking component into: #{want}")

        case File.ln_s(component_dir, want) do
          :ok -> :ok
          {:error, reason} -> die("Failed to create symlink: #{want} (#{inspect(reason)})")
        end
    end
  end

  defp remove_component_link!(component_dir, esp32_dir) do
    if not File.dir?(component_dir), do: die("Component directory not found: #{component_dir}")

    name = Path.basename(component_dir)
    want = Path.join([esp32_dir, @components_dirname, name])

    cond do
      symlink?(want) ->
        want_real = canonical_path(want)
        component_real = canonical_path(component_dir)

        cond do
          present?(want_real) and present?(component_real) and want_real == component_real ->
            File.rm!(want)
            say("✔ removed component symlink: #{want}")

          present?(want_real) and present?(component_real) ->
            die(
              "Refusing to remove component symlink because it points elsewhere: #{want} -> #{want_real}"
            )

          true ->
            die(
              "Refusing to remove component symlink because its target could not be resolved: #{want}"
            )
        end

      File.exists?(want) ->
        die("Refusing to remove component path because it is not a symlink: #{want}")

      true ->
        say("Component symlink not present: #{want}")
    end
  end

  defp show_component_status!(component_dir, esp32_dir) do
    name = Path.basename(component_dir)
    want = Path.join([esp32_dir, @components_dirname, name])

    say("")
    say("Component")
    say("- component_dir: #{component_dir}")
    say("- link_path:     #{want}")

    cond do
      not File.dir?(component_dir) ->
        say("- status:        component directory missing")

      symlink?(want) ->
        want_real = canonical_path(want)
        component_real = canonical_path(component_dir)

        cond do
          present?(want_real) and present?(component_real) and want_real == component_real ->
            say("- status:        linked to this component")
            say("- target:        #{want_real}")

          present?(want_real) ->
            say("- status:        linked elsewhere")
            say("- target:        #{want_real}")

          true ->
            say("- status:        symlink target could not be resolved")
        end

      File.exists?(want) ->
        say("- status:        path exists but is not a symlink")

      true ->
        say("- status:        not linked")
    end
  end

  defp build_boot_avm_if_needed!(shared) do
    boot_avm = Path.join(shared.atomvm_root, boot_avm_rel_path(shared))

    unless File.regular?(boot_avm) do
      require_cmd!("cmake")
      say("Generating boot AVM (Generic UNIX build)")
      build_dir = Path.join(shared.atomvm_root, "build")
      File.mkdir_p!(build_dir)
      run!("cmake", ["-S", shared.atomvm_root, "-B", build_dir], env: host_tool_env())
      run!("cmake", ["--build", build_dir], env: host_tool_env())
      ensure_regular_file!(boot_avm, "boot AVM")
    end
  end

  defp boot_avm_rel_path(shared) do
    if shared.elixir_support do
      @elixir_boot_avm_rel_path
    else
      @erlang_boot_avm_rel_path
    end
  end

  defp build_host_tree!(atomvm_root, host_build_dir) do
    require_cmd!("cmake")
    File.mkdir_p!(host_build_dir)
    run!("cmake", ["-S", atomvm_root, "-B", host_build_dir], env: host_tool_env())
    run!("cmake", ["--build", host_build_dir], env: host_tool_env())
  end

  defp build_platform_tree!(shared, platform_build_dir, opts) do
    release = Keyword.get(opts, :release, false)
    cmake_args = platform_cmake_args(shared, release: release)

    run_idf!(
      shared.esp32_dir,
      ["-B", platform_build_dir] ++ cmake_args ++ ["set-target", shared.target],
      sdkconfig_env(shared)
    )

    run_idf!(
      shared.esp32_dir,
      ["-B", platform_build_dir, "build"],
      sdkconfig_env(shared)
    )
  end

  defp platform_cmake_args(shared, opts) do
    release = Keyword.get(opts, :release, false)

    []
    |> maybe_append(shared.elixir_support, "-DATOMVM_ELIXIR_SUPPORT=on")
    |> maybe_append(release, "-DATOMVM_RELEASE=on")
    |> Kernel.++(platform_compatibility_cmake_args(shared))
    |> Kernel.++(Enum.map(shared.cmake_defines, &normalize_cmake_define/1))
  end

  defp platform_compatibility_cmake_args(%{target: target})
       when target in @atomvm_dual_core_xtensa_targets do
    [@atomvm_idf_atomic_probe_override]
  end

  defp platform_compatibility_cmake_args(_shared), do: []

  defp maybe_append(args, true, value), do: args ++ [value]
  defp maybe_append(args, false, _value), do: args

  defp component_sdkconfigs(components, target) do
    Enum.flat_map(components, fn component ->
      [
        Path.join(component, @sdkconfig_defaults_filename),
        Path.join([component, @sdkconfig_defaults_dirname, @sdkconfig_defaults_filename]),
        Path.join(component, "#{@sdkconfig_defaults_filename}.#{target}"),
        Path.join([
          component,
          @sdkconfig_defaults_dirname,
          "#{@sdkconfig_defaults_filename}.#{target}"
        ])
      ]
    end)
    |> Enum.filter(&File.regular?/1)
    |> Enum.map(&Path.expand/1)
  end

  defp local_sdkconfigs(target) do
    component_sdkconfigs([File.cwd!()], target)
  end

  defp linked_components(esp32_dir) do
    components_dir = Path.join(esp32_dir, @components_dirname)

    case File.ls(components_dir) do
      {:ok, entries} ->
        entries
        |> Enum.map(&Path.join(components_dir, &1))
        |> Enum.filter(&File.dir?/1)
        |> Enum.map(fn path ->
          case canonical_path(path) do
            "" -> Path.expand(path)
            real_path -> real_path
          end
        end)

      {:error, _reason} ->
        []
    end
  end

  defp uniq_paths(paths) do
    Enum.uniq_by(paths, &Path.expand/1)
  end

  defp normalize_cmake_define(value) do
    value = to_string(value)
    if String.starts_with?(value, "-D"), do: value, else: "-D#{value}"
  end

  defp sdkconfig_env(shared) do
    defaults = [@sdkconfig_defaults_filename | shared.sdkconfigs]
    %{"SDKCONFIG_DEFAULTS" => Enum.join(defaults, ";")}
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
          - This script currently expects #{env_expected_idf_hint()} for AtomVM ESP32 builds.
          - Check the selected EIM version with: eim list
          - Retry after cleaning if the build dir is stale:
              #{@script_name} clean
        """)
    end
  end

  defp env_expected_idf_hint do
    System.get_env("ATOMVM_IDF_VERSION", @recommended_idf_version) <> ".x"
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

  defp maybe_copy_image_to_out_dir!(image_path, ""), do: image_path
  defp maybe_copy_image_to_out_dir!(image_path, nil), do: image_path

  defp maybe_copy_image_to_out_dir!(image_path, out_dir) do
    File.mkdir_p!(out_dir)
    output_path = Path.join(out_dir, Path.basename(image_path))
    File.cp!(image_path, output_path)
    say("✔ copied image: #{output_path}")
    output_path
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
    lsof_output = serial_port_lsof_output(port)
    fuser_output = serial_port_fuser_output(port)

    cond do
      present?(lsof_output) -> "Detected by lsof:\n" <> lsof_output
      present?(fuser_output) -> "Detected by fuser:\n" <> fuser_output
      true -> nil
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

  defp display_list([]), do: "(none)"
  defp display_list(values), do: Enum.join(values, ", ")

  defp expand_optional_path(value) do
    value = to_string(value || "") |> String.trim()
    if value == "", do: "", else: Path.expand(value)
  end

  defp present?(value), do: is_binary(value) and String.trim(value) != ""

  defp first_present(value, fallback) do
    if present?(value), do: value, else: fallback
  end

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
    ref = ref |> to_string() |> String.trim()

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

  defp host_tool_env, do: %{}

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
        normalized = value |> String.trim() |> String.downcase()
        normalized in ["1", "true", "yes", "y", "on"]
    end
  end

  defp die_unknown_ref!(repo_dir, ref) do
    die("""
    Could not resolve AtomVM ref: #{ref}

    Expected one of:
      - branch name (e.g. main)
      - tag name (e.g. v0.6.6)
      - full SHA (40 hex chars)

    Check the local AtomVM repo and remote manually:
      git -C #{shell_display(repo_dir)} remote -v
      git -C #{shell_display(repo_dir)} ls-remote --heads #{@atomvm_git_url} #{ref}
      git -C #{shell_display(repo_dir)} fetch --depth 1 #{@atomvm_git_url} refs/heads/#{ref}
    """)
  end
end

Main.main(System.argv())
