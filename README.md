# VStore: A Data Store for Analytics on Large Videos
VStore is a data store for supporting fast, resource efficient analytics over large archival videos.
Please read our paper in EuroSys '19: [VStore: A Data Store for Analytics on Large Videos](https://web.ics.purdue.edu/~xu944/eurosys19.pdf)
and visit out [website](https://thexsel.github.io/p/vstore/) for more details.

## Overview
This source code orchestrates VStore's ingestion, storage, retrieval, and consumption based on 2 modern computer vision pipelines, [OpenALPR](https://github.com/openalpr/openalpr) and [NoScope](https://github.com/stanford-futuredata/noscope).
VStore's configurations, including the derivation of consumption formats, the derivation of storage formats, and data erosion are not included.

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
#### 1. Ingestion (Build video footage by transcoding)
First, transcode the videos to the target formats: Follow the guides [here](https://gist.github.com/tiantuxu/6dca1b86f5ad5f7386d242f001a1cf08).

To generate raw video footage, 
```
ffmpeg -i input.mp4 -c:v rawvideo -pix_fmt yuv420p video-raw.yuv
```
To generate encoded video chunks, 
```
ffmpeg -i input.mp4 -acodec copy -f segment -vcodec copy -reset_timestamps 1 -map 0 ./video-chunks/output%04d.mp4
```
#### 2. Storage
Second, build the footage to lmdb:

for raw video footage,
```
/tmp/teddyxu/Debug/test-db-build.bin -r --dpath=/path/to/database --vpath=/path/to/video-raw.yuv width height
```
for encoded video chunks,
```
 /tmp/teddyxu/Debug/test-db-build.bin -e --dpath=/path/to/database --vpath=/path/to/video-chunks width height
``` 
#### 3. Retrieval (Run source and put the decoder online)
```
/tmp/teddyxu/Debug/test-source.bin
/tmp/teddyxu/Debug/decode-serv.bin
```
#### 4. Consumption (sink)
For OpenALPR, revoke OpenALPR sink by running
```
/tmp/teddyxu/Debug/test-sink.bin
```

For NoScope, follow their isntructions and download the videos/csv/YOLO weights and build the sink by bazel as described in [NoScope GitHub Page](https://github.com/stanford-futuredata/noscope).
After building NoScope sink, revoke the sink by running sample a command like
```
./VStore-NoScope/data/experiments/jackson-town-square/0.02/run_testset.sh 0 VIDEO_LEN RESOLUTION
```
