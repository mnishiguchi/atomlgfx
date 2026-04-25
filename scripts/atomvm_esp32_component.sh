#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

atomvm_root="${ATOMVM_ROOT:-${HOME}/atomvm/AtomVM}"
atomvm_esp32_dir="${atomvm_root}/src/platforms/esp32"
components_dir="${atomvm_esp32_dir}/components"
component_name="$(basename "${project_root}")"
component_path="${components_dir}/${component_name}"

command="${1:-link}"

print_usage() {
  cat <<EOF
Usage:
  $(basename "$0") [command]

Commands:
  link      Link this project into AtomVM ESP32 components
  unlink    Remove the component symlink if it points to this project
  status    Show current component link status

Environment:
  ATOMVM_ROOT=/path/to/AtomVM
EOF
}

ensure_atomvm_esp32_dir() {
  if [ ! -d "${atomvm_root}/.git" ]; then
    echo "AtomVM repo not found: ${atomvm_root}" >&2
    echo "Set ATOMVM_ROOT=/path/to/AtomVM if needed." >&2
    exit 1
  elif [ ! -d "${atomvm_esp32_dir}" ]; then
    echo "AtomVM ESP32 platform dir not found: ${atomvm_esp32_dir}" >&2
    exit 1
  else
    mkdir -p "${components_dir}"
  fi
}

show_status() {
  echo "Project root:"
  echo "  ${project_root}"
  echo
  echo "AtomVM root:"
  echo "  ${atomvm_root}"
  echo
  echo "Component path:"
  echo "  ${component_path}"
  echo

  if [ -L "${component_path}" ]; then
    linked_path="$(realpath "${component_path}")"
    project_real_path="$(realpath "${project_root}")"

    if [ "${linked_path}" = "${project_real_path}" ]; then
      echo "Status:"
      echo "  linked to this project"
    else
      echo "Status:"
      echo "  linked elsewhere"
      echo
      echo "Current target:"
      echo "  ${linked_path}"
    fi
  elif [ -e "${component_path}" ]; then
    echo "Status:"
    echo "  path exists, but it is not a symlink"
  else
    echo "Status:"
    echo "  not linked"
  fi
}

link_component() {
  ensure_atomvm_esp32_dir

  if [ -L "${component_path}" ]; then
    linked_path="$(realpath "${component_path}")"
    project_real_path="$(realpath "${project_root}")"

    if [ "${linked_path}" = "${project_real_path}" ]; then
      echo "Component symlink already exists:"
      echo "  ${component_path} -> ${project_root}"
    else
      echo "Component symlink points elsewhere:" >&2
      echo "  ${component_path} -> ${linked_path}" >&2
      echo "Expected:" >&2
      echo "  ${project_root}" >&2
      exit 1
    fi
  elif [ -e "${component_path}" ]; then
    echo "Component path exists but is not a symlink:" >&2
    echo "  ${component_path}" >&2
    exit 1
  else
    ln -s "${project_root}" "${component_path}"
    echo "Linked AtomVM ESP32 component:"
    echo "  ${component_path} -> ${project_root}"
  fi
}

unlink_component() {
  ensure_atomvm_esp32_dir

  if [ -L "${component_path}" ]; then
    linked_path="$(realpath "${component_path}")"
    project_real_path="$(realpath "${project_root}")"

    if [ "${linked_path}" = "${project_real_path}" ]; then
      rm "${component_path}"
      echo "Removed AtomVM ESP32 component symlink:"
      echo "  ${component_path}"
    else
      echo "Refusing to remove symlink because it points elsewhere:" >&2
      echo "  ${component_path} -> ${linked_path}" >&2
      echo "Expected:" >&2
      echo "  ${project_root}" >&2
      exit 1
    fi
  elif [ -e "${component_path}" ]; then
    echo "Refusing to remove path because it is not a symlink:" >&2
    echo "  ${component_path}" >&2
    exit 1
  else
    echo "Component symlink does not exist:"
    echo "  ${component_path}"
  fi
}

case "${command}" in
link)
  link_component
  ;;

unlink)
  unlink_component
  ;;

status)
  show_status
  ;;

-h | --help | help)
  print_usage
  ;;

*)
  print_usage >&2
  echo >&2
  echo "Unknown command: ${command}" >&2
  exit 2
  ;;
esac
