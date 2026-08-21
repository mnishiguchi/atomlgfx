# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.MixProject do
  use Mix.Project

  # AtomLGFX は未公開だが、Mix が SemVer 形式の値を要求するため仮の版を置く。
  @version "0.1.0"

  def project do
    [
      app: :atomlgfx,
      version: @version,
      elixir: "~> 1.17",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      description: description(),
      package: package(),
      homepage_url: source_url(),
      source_url: source_url(),
      docs: docs()
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp deps do
    [
      {:ex_doc, "~> 0.40", only: :dev, runtime: false}
    ]
  end

  defp description do
    "AtomVM 用 LovyanGFX ドライバーの Elixir ラッパー"
  end

  defp package do
    [
      name: "atomlgfx",
      licenses: ["Apache-2.0"],
      files: [
        "lib",
        "mix.exs",
        "README.md",
        "CHANGELOG.md",
        "LICENSES"
      ],
      links: %{
        "GitHub" => "https://github.com/mnishiguchi/atomlgfx"
      }
    ]
  end

  defp docs do
    [
      main: "AtomLGFX",
      source_ref: "main",
      source_url: source_url(),
      extras: [
        "README.md",
        "CHANGELOG.md",
        "docs/architecture.md",
        "docs/adr/0015-v3-low-memory-protocol.md",
        "docs/adr/0016-render-first-low-memory-api.md",
        "docs/adr/0017-direct-atomvm-nif-api.md",
        "docs/adr/0018-optional-legacy-port.md",
        "docs/boards/m5stack.md",
        "docs/elixir-package.md",
        "docs/esp-idf-component.md",
        "docs/migration-to-v3.md",
        "docs/protocol.md",
        "docs/protocol-reference.md",
        "docs/worklog/20260821-nif-only-hardware-validation-report.md"
      ]
    ]
  end

  defp source_url, do: "https://github.com/mnishiguchi/atomlgfx"
end
