# shell.nix

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    llvmPackages.clang
    llvmPackages.lld
    llvmPackages.llvm

    gnumake
  ];

  buildInputs = with pkgs; [ musl ];
}
