{pkgs, compiler_environment}:
let
  xrpl = pkgs.callPackage ./xrpl.nix { inherit compiler_environment; };
  cassandra_cpp_driver_build = pkgs.callPackage ./cassandra_cpp_driver.nix { };
in
  pkgs.mkShell {
    packages = with pkgs; [
        python3Full
        cmake
        clang-tools_16
    # dependencies
        boost182
        cassandra_cpp_driver_build
        fmt
        openssl_1_1
        xrpl
    ];
  }
