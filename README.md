# VStore: A Data Store for Analytics on Large Videos
Please read our paper in EuroSys '19: [VStore: A Data Store for Analytics on Large Videos](https://web.ics.purdue.edu/~xu944/eurosys19.pdf)
and visit out [website](https://thexsel.github.io/p/vstore/).

## Overview
VStore is a data store for supporting fast, resource efficient analytics over large archival videos. 
The code is built opon 2 modern computer vision pipelines, [OpenALPR](https://github.com/openalpr/openalpr) and [NoScope](https://github.com/stanford-futuredata/noscope).

## Build VStore
#### 1. VStore Requirements
To build VStore, you need the following to be installed
* ZeroMQ 2.2.x
* lmdb
#### 2. Benchmark Requirements
Follow the requirement of [OpenALPR](https://github.com/openalpr/openalpr) and [NoScope](https://github.com/stanford-futuredata/noscope).
#### 3. Build
```
cd ./src
source env-teddy.sh
mkdir cmake-build-debug; cd cmake-build-debug; cmake .. -DCMAKE_BUILD_TYPE=Debug; cd ..
mkdir cmake-build-release; cd cmake-build-release; cmake .. -DCMAKE_BUILD_TYPE=Release; cd ..
build-all
```

## Run VStore
#### 1. Build Video Footage
First, transcode the videos to the target formats: Follow the guides [here](https://gist.github.com/tiantuxu/6dca1b86f5ad5f7386d242f001a1cf08).

Second, build the footage to lmdb:

for raw video footage,
```
/tmp/vstore/Debug/test-db-build.bin -r --dpath=/path/to/database --vpath=/path/to/video.yuv width height
```
for encoded video chunks,
```
 /tmp/vstore/Debug/test-db-build.bin -e --dpath=/path/to/database --vpath=/path/to/video.yuv width height
``` 
#### 2. Run Source
```
/tmp/vstore/Debug/test-source.bin
```
#### 3. Run Decoder
```
/tmp/vstore/Debug/decode-serv.bin
```
#### 4. Run Sink
For OpenALPR, After building NoScope sink, revoke OpenALPR sink by running
```
/tmp/vstore/Debug/test-sink.bin
```

For NoScope, follwo their isntructions and download the videos/csv/YOLO weights and build the sink by bazel as described in [NoScope GitHub Page](https://github.com/stanford-futuredata/noscope).
After building NoScope sink, revoke the sink by running sample a command like
```
./VStore-NoScope/data/experiments/jackson-town-square/0.02/run_testset.sh 0 VIDEO_LEN RESOLUTION
```
