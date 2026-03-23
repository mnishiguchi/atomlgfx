# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.MixProject do
  use Mix.Project

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
        "LICENSE",
        "docs"
      ],
      links: %{
        "GitHub" => "https://github.com/mnishiguchi/atomlgfx"
      }
    ]
  end

  defp docs do
    [
      main: "AtomLGFX",
      extras: [
        "README.md"
      ]
    ]
  end
end
