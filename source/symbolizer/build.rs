fn main() {
    let proto_dir = "../proto";
    let proto_file = "snapshot.proto";

    println!("cargo:rerun-if-changed={}", proto_dir);
    println!("cargo:rerun-if-changed=build.rs");

    prost_build::Config::new()
        .out_dir("src/proto/gen")
        .type_attribute(".", "#[derive(bitcode::Encode, bitcode::Decode)]")
        .compile_protos(&[format!("{}/{}", proto_dir, proto_file)], &[proto_dir])
        .unwrap();
}
