# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.MixProject do
  use Mix.Project

  # Mix requires a SemVer value even though AtomLGFX has no package release yet.
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
    "Elixir wrapper for the AtomVM LovyanGFX driver"
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
        "docs/elixir-package.md",
        "docs/esp-idf-component.md",
        "docs/migration-to-v3.md",
        "docs/protocol-reference.md"
      ]
    ]
  end

  defp source_url, do: "https://github.com/mnishiguchi/atomlgfx"
end
