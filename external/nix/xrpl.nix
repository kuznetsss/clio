{
  compiler_environment,
  fetchFromGitHub,
  cmake, pkg-config, git,
  boost182,
  howard-hinnant-date,
  grpc,
  libarchive,
  lz4,
  openssl_1_1,
  protobuf3_21,
  snappy,
  soci,
  sqlite,
  zlib,
  lib
}: let
    nudb = fetchFromGitHub {
        owner = "cppalliance";
        repo = "NuDB";
        rev = "refs/tags/2.0.8";
        sha256 = "sha256-DdYRGKTDJodGHwI49D654/k1QO4+1vH1m3KfFL9I+S0=";
    };

in compiler_environment.mkDerivation rec {
    pname = "xrpl";
    version = "1.12.0";

    src = fetchFromGitHub {
      owner = "XRPLF";
      repo = "rippled";
      rev = "refs/tags/${version}";
      sha256 = "sha256-8LQo3Aco7afOqPLJaesHrlkLB0qo+rR9cYXFfCzuCfc=";
    };

    # cmakeFlags = [
    #     "-DBOOST_ROOT=${boost182.dev}"
    #     "-DOPENSSL_ROOT_DIR=${openssl_1_1.dev}"
    #     "-DOPENSSL_USE_STATIC_LIBS=TRUE"
    # ];
    nativeBuildInputs = [ cmake pkg-config git ];
    buildInpus = [
        lz4
        boost182
        boost182.dev
        howard-hinnant-date
        grpc
        libarchive
        nudb
        openssl_1_1
        openssl_1_1.dev
        protobuf3_21
        snappy
        soci
        sqlite
        zlib
        cmake
    ];
}
