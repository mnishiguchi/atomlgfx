# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.MixProject do
  use Mix.Project

  @version "0.1.0"
  @source_url "https://github.com/mnishiguchi/atomlgfx"

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
      # Hex には Elixir API と利用ガイドを含め、NIF は同じタグから構築する。
      files: [
        "lib",
        "mix.exs",
        "README.md",
        "CHANGELOG.md",
        "docs/getting-started.md",
        "docs/elixir-package.md",
        "docs/esp-idf-component.md",
        "docs/compatibility.md",
        "LICENSES"
      ],
      links: %{
        "GitHub" => @source_url
      }
    ]
  end

  defp docs do
    [
      main: "AtomLGFX.LGFX",
      source_ref: "v#{@version}",
      source_url: source_url(),
      extras: [
        "README.md",
        "CHANGELOG.md",
        "docs/getting-started.md",
        "docs/elixir-package.md",
        "docs/esp-idf-component.md",
        "docs/compatibility.md"
      ]
    ]
  end

  defp source_url, do: @source_url
end
