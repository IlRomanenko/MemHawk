# Grafana integration

MemHawk can write aggregated data without a symbolification step as protobufs. Such output is enabled if option `memhawk.writers.proto_writer.enabled` is set to `true`; right now it's **enabled** by default.

File format is described in the last paragraph.

## Usage

### Step 1: Download latest release or build MemHawk

Download the latest release (can be acquired from the GitHub release page) and unpack it. Alternatively, build MemHawk locally and install artifacts into the necessary directory.

### Step 2: Start Grafana and ClickHouse

Start containers by running `docker-compose up -d` inside the `monitoring` folder of the installation.

**Cheatsheet:**

* `docker-compose up` - start with capturing stdout of containers
* `docker-compose up -d` - start as daemon
* `docker-compose down` - stop containers
* `docker-compose down -v` - stop containers and remove all volumes (aka data)

### Step 3: Run application under profiler

* Using LD_PRELOAD

```bash
LD_PRELOAD=./memhawk/lib/libmemhawk.so <your_application>
```

* Using patchelf for binaries with suid/sgid bit

```bash
patchelf --add-needed ./memhawk/lib/libmemhawk.so <your_application>
<your_application> <your app args>
```

### Step 4: Symbolize gathered profile

```bash
./memhawk/bin/symbolizer processor -f memhawk_<process_name>_<process_pid>_protobuf.binpb --watch
```

### Step 5: Inspect memory profile

Open Grafana UI: <http://localhost:3000>, user - `admin`, password - `admin`

## File format

Protobuf messages are defined in [snapshot.proto](../source/proto/snapshot.proto).

```text
<binpb> ::= <header> <snapshots>
<snapshots> ::= <snapshot> | <snapshot> <snapshots>
<header> ::= <record(ProcessInfo)>
<snapshot> ::= <record(Snapshot)>

<record(Msg)> ::= <zstd_compressed_size(Msg)> <zstd_compressed_payload(Msg)>
<zstd_compressed_size> ::= 8 bytes (big-endian uint64); size of zstd compressed payload
<zstd_compressed_payload> ::= serialized protobuf message after zstd compression 
```

To read the file, first read 8 bytes to get the compressed size, then read that many bytes, decompress with zstd, and parse as a corresponding protobuf message.
The first message is ProcessInfo, followed by a sequence of Snapshot messages.
