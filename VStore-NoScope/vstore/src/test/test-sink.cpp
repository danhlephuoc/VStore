//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

/*
 * Copyright (c) 2015 OpenALPR Technology, Inc.
 * Open source Automated License Plate Recognition [http://www.openalpr.com]
 *
 * This file is part of OpenALPR.
 *
 * OpenALPR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef DEBUG
#define GLIBCXX_DEBUG 1 // debugging
#endif

#include <sstream> // std::ostringstream

#include <zmq.hpp>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <unistd.h>
#include "config.h"
#include "vs-types.h"
#include "mydecoder.h"
#include "log.h"
#include "rxtx.h"
#include "RxManager.h"
#include "measure.h"

#include "StatCollector.h"
#include "CallBackTimer.h"

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/core/operations.hpp"
#include "opencv2/core/core.hpp"

#include "tclap/CmdLine.h"
#include "openalpr/support/filesystem.h"
#include "openalpr/support/timing.h"
#include "openalpr/support/platform.h"
#include "video/videobuffer.h"
#include "openalpr/motiondetector.h"
#include "openalpr/alpr.h"
#include <queue>
#include "rt-config.h"

extern "C"{
#include <omp.h>
};

using namespace vs;
using namespace alpr;

const std::string MAIN_WINDOW_NAME = "ALPR main window";

const bool SAVE_LAST_VIDEO_STILL = false;
const std::string LAST_VIDEO_STILL_LOCATION = "/tmp/laststill.jpg";
const std::string WEBCAM_PREFIX = "/dev/video";
//MotionDetector motiondetector;
MotionDetector motiondetector[64];
bool do_motiondetection = true;

/** Function Headers */
bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool writeJson);
bool is_supported_image(std::string image_file);

bool measureProcessingTime = true;
std::string templatePattern;

// This boolean is set to false when the user hits terminates (e.g., CTRL+C )
// so we can end infinite loops for things like video processing.
bool program_active = true;

bool flag_180[50];
bool flag_720[50]={true};                           //flag indicate that it is currently feeding 720p video chunk to the client
bool flag_540[50];
const int chunk_num = 1;
const int chunk_num_encode = 1;

static shared_ptr<zmq::message_t> pre_buf_180[50][PREFETCH_BUF];
static shared_ptr<zmq::message_t> pre_buf_720[50][PREFETCH_BUF];
static shared_ptr<zmq::message_t> pre_buf_540[50][PREFETCH_BUF];

static shared_ptr<zmq::message_t> pre_enbuf_180[50][CHUNK_LENGTH];
static shared_ptr<zmq::message_t> pre_enbuf_720[50][CHUNK_LENGTH];
static shared_ptr<zmq::message_t> pre_enbuf_540[50][CHUNK_LENGTH];
/*
static StatCollector stat;

static void getStatistics(void) {
	Statstics s;
	if (!stat.ReportStatistics(&s))
		return;

	if (s.rec_persec < 1) // nothing?
		return;

	printf("%.2f MB/sec \t %.0f fps \n", s.byte_persec/1024/1024, s.rec_persec);
};
*/

/* pull the source to get info for all streams */
void getAllStreamInfo() {

}

atomic<int> mycount(0);
atomic<int> mycount_rr(0);
atomic<int> mycount_ocr(0);

atomic<int> count1(0);
atomic<int> count2(0);
atomic<int> count3(0);

atomic<int> chunk_count(0);

std::vector<AlprRegionOfInterest> detectmotion( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson, int proc_id, int f_seq, int reso)
//bool detectandshow( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson)
{
    timespec startTime;
    timespec endTime;

    //count1 ++;

    std::vector<AlprRegionOfInterest> regionsOfInterest;
#if 1
    //int ratio = 720 / 180;
#endif
    /*
    if (do_motiondetection)
    {
        cout << "motion detection" << endl;
        cv::Rect rectan = motiondetector.MotionDetect(&frame);
        if (rectan.width>0) regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
    }
    else regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));
    */

    /* Motion detection phase */
    cv::Rect rectan = motiondetector[proc_id].MotionDetect(&frame);
    if (rectan.width > 0) {
        //cout << "here" << endl;
        //regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
        //regionsOfInterest.push_back(AlprRegionOfInterest(reso*rectan.x/720, reso*rectan.y/720, reso*rectan.width/720, reso*rectan.height/720));
        regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
    }

    cout << rectan.x << " " << rectan.y << " " << rectan.width << " " << rectan.height << " " << endl;
    //std::cout << "Motion Detection: " << diffclock(startTime, endTime) << "ms." << std::endl;

    return regionsOfInterest;
}

//bool recognizeplate( Alpr* alpr, cv::Mat frame_720, std::string region, bool writeJson, int proc_id, int f_seq, std::vector<AlprRegionOfInterest> regionsOfInterest){
std::queue<PlateRegion> recognizeRegion( Alpr* alpr, cv::Mat frame_720, std::string region, bool writeJson, int proc_id, int f_seq, std::vector<AlprRegionOfInterest> regionsOfInterest, int reso) {

    AlprResults results;
    std::queue<PlateRegion> plateQueue;
    timespec startTime;
    timespec endTime;
    /* Recognition phase */
    //getTimeMonotonic(&startTime);
    /*
    int arraySize = frame_720.cols * frame_720.rows * frame_720.elemSize();
    cv::Mat imgData = cv::Mat(arraySize, 1, CV_8U, frame_720.data);
    cv::Mat img = imgData.reshape(frame_720.elemSize(), frame_720.elemSize());

    if (regionsOfInterest.size() == 0) {
        AlprRegionOfInterest fullFrame(0, 0, img.cols, img.rows);
        regionsOfInterest.push_back(fullFrame);
    }
    */

    //results = alpr->recognize(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, frame_ocr, regionsOfInterest);
    //splateQueue = alpr->recognizeregion(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, regionsOfInterest);

    plateQueue =  alpr->recognizeregion(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, regionsOfInterest);

    //I("%d platequeue.size = %lu", f_seq, plateQueue.size());
    //cout << f_seq << " " << plateQueue.size() << endl;
    std::queue<PlateRegion> plateQueueReturn;

    while(!plateQueue.empty()) {
        PlateRegion plateRegion = plateQueue.front();
        plateQueue.pop();

            plateQueue.front().rect.x = int(reso*(plateQueue.front().rect.x)/720);
            plateQueue.front().rect.y = int(reso*(plateQueue.front().rect.y)/720);
            plateQueue.front().rect.width = int(reso*(plateQueue.front().rect.width)/720);
            plateQueue.front().rect.height = int(reso*(plateQueue.front().rect.height)/720);
    }

    //std::queue<PlateRegion> plateQueueReturn;

    //results = alpr->recognize(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, regionsOfInterest);

    /* print the ROI */
    //cout << regionsOfInterest[0].x << " " << regionsOfInterest[0].y << " " << regionsOfInterest[0].width << " " << regionsOfInterest[0].height << " " << endl;

    //getTimeMonotonic(&endTime);
    //std::cout << "Total Plate Recognition: " << diffclock(startTime, endTime) << "ms." << std::endl;
    count2 ++;

    return plateQueue;

    /*
    if (measureProcessingTime) {
        std::cout << "Total Time to process image: " << totalProcessingTime << "ms." << std::endl;
    }
    */
}

bool recognizeplate( Alpr* alpr, cv::Mat frame_720, std::string region, bool writeJson, int proc_id, int f_seq, std::vector<AlprRegionOfInterest> regionsOfInterest)
{
    AlprResults results;

    results = alpr->recognize(frame_720.data, frame_720.elemSize(), frame_720.cols, frame_720.rows, regionsOfInterest);
    if (writeJson)
    {
        std::cout << alpr->toJson( results ) << std::endl;
    }
    else
    {
        for (int i = 0; i < results.plates.size(); i++)
        {
            std::cout << "plate" << i << ": " << results.plates[i].topNPlates.size() << " results";
            if (measureProcessingTime)
                std::cout << " -- Processing Time = " << results.plates[i].processing_time_ms << "ms.";
            std::cout << std::endl;

            if (results.plates[i].regionConfidence > 0)
                std::cout << "State ID: " << results.plates[i].region << " (" << results.plates[i].regionConfidence << "% confidence)" << std::endl;

            for (int k = 0; k < results.plates[i].topNPlates.size(); k++)
            {
                // Replace the multiline newline character with a dash
                std::string no_newline = results.plates[i].topNPlates[k].characters;
                std::replace(no_newline.begin(), no_newline.end(), '\n','-');

                std::cout << "    - " << no_newline << "\t confidence: " << results.plates[i].topNPlates[k].overall_confidence;
                if (templatePattern.size() > 0 || results.plates[i].regionConfidence > 0)
                    std::cout << "\t pattern_match: " << results.plates[i].topNPlates[k].matches_template;

                std::cout << std::endl;
            }
        }
    }

    return results.plates.size() > 0;
}

bool recognizePlate( Alpr* alpr, cv::Mat frame_ocr, cv::Mat frame_ocr_gray, std::string region, bool writeJson, int proc_id, int f_seq, queue<PlateRegion> plateQueue)
{
    AlprResults results;

    //cout << plateQueue.front().rect.x << endl;
    //cout << frame_ocr.cols << endl;
    //for(auto iter = plateQueue.front(); iter != plateQueue.back(); iter ++){
    //    cout << plateQueue.front().rect.x << endl;
    //    cout << plateQueue.front().rect.y << endl;
    //    cout << plateQueue.front().rect.width << endl;
    //    cout << plateQueue.front().rect.height << endl;

    //}

    timespec startTime;
    timespec endTime;

    getTimeMonotonic(&startTime);

    results = alpr->recognizeresult(frame_ocr, frame_ocr_gray, plateQueue);

    if (writeJson)
    {
        std::cout << alpr->toJson( results ) << std::endl;
    }
    else
    {
        for (int i = 0; i < results.plates.size(); i++)
        {
            std::cout << "plate" << i << ": " << results.plates[i].topNPlates.size() << " results";
            if (measureProcessingTime)
                std::cout << " -- Processing Time = " << results.plates[i].processing_time_ms << "ms.";
            std::cout << std::endl;

            if (results.plates[i].regionConfidence > 0)
                std::cout << "State ID: " << results.plates[i].region << " (" << results.plates[i].regionConfidence << "% confidence)" << std::endl;

            for (int k = 0; k < results.plates[i].topNPlates.size(); k++)
            {
                // Replace the multiline newline character with a dash
                std::string no_newline = results.plates[i].topNPlates[k].characters;
                std::replace(no_newline.begin(), no_newline.end(), '\n','-');

                std::cout << "    - " << no_newline << "\t confidence: " << results.plates[i].topNPlates[k].overall_confidence;
                if (templatePattern.size() > 0 || results.plates[i].regionConfidence > 0)
                    std::cout << "\t pattern_match: " << results.plates[i].topNPlates[k].matches_template;

                std::cout << std::endl;
            }
        }
    }

    getTimeMonotonic(&endTime);
    count3 ++;

    return results.plates.size() > 0;
}


void init_raw_chunks(int j, int id, int *status, shared_ptr<zmq::message_t> prefetch[][chunk_num*CHUNK_SIZE], zmq::socket_t **work_skt, int db_seq, request_desc *rd_one_frame, bool *flag){

    zmq::message_t request(sizeof(request_desc));

    memcpy(request.data(), rd_one_frame, sizeof(request_desc));

    /********* Initializing 5 CHUNKS of Raw Frames **********/
    if(j-status[id] > chunk_num*CHUNK_SIZE){
        for(int i=0;i<chunk_num;i++)
        {
            work_skt[id]->send(request);
            request_desc *rd;
            rd = (request_desc *)(request.data()), request.size();
            I("Initial request sent, start: %d, frame: %d", rd->start_fnum, rd->total_fnum);

            for (int iter = 0; iter < rd->total_fnum; iter++) {
                auto msg_ptr = recv_one_chunk_720(*work_skt[id]);
                prefetch[id][iter+CHUNK_SIZE*i] = msg_ptr;
            }
            rd->db_seq = db_seq;
            rd->start_fnum = j + CHUNK_SIZE*(i+1);
            rd->total_fnum = CHUNK_SIZE;
            memcpy(request.data(), &rd, sizeof(request_desc));

        }
        status[id] = j;
        flag[id]=false;
    }

    if(j-status[id]<=CHUNK_SIZE)
        flag[id]=false;

}


int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

    zmq::socket_t q_request(context, ZMQ_REQ);
    q_request.connect(WORKER_REQUEST);

    zmq::socket_t s_request(context, ZMQ_REQ);
    s_request.connect(SERVICE_PUSH_ADDR);

    zmq::socket_t *service_socket[50];
    for(int worker_count = 0; worker_count < 50; worker_count++){
        service_socket[worker_count]= new zmq::socket_t(context, ZMQ_REQ);
        service_socket[worker_count]->connect(SERVICE_PUSH_ADDR);
    }


	CallBackTimer stat_collector;
	RxManager mg;

	//stat_collector.start(200 /*ms, check interval */, &getStatistics);

	/* consumption phase */
	//Frame f;
	int rc;
	seq_t wm_c, wm_f;

    /* teddyxu: alpr starts here */
    //std::vector<std::string> filenames;
    std::string configFile = "";
    bool outputJson = false;
    //int seektoms = 0;
    //bool detectRegion = false;
    std::string country;
    int topn;

    timespec startTime;
    timespec endTime;

    timespec started;
    timespec ended;
    //bool debug_mode = false;

    TCLAP::CmdLine cmd("OpenAlpr Command Line Utility", ' ', Alpr::getVersion());
    TCLAP::UnlabeledMultiArg<std::string> fileArg("image_file", "Image containing license plates", true, "",
                                                      "image_file_path");
    TCLAP::ValueArg<std::string> countryCodeArg("c", "country",
                                                "Country code to identify (either us for USA or eu for Europe).  Default=us",
                                                false, "us", "country_code");
    TCLAP::ValueArg<int> seekToMsArg("", "seek", "Seek to the specified millisecond in a video file. Default=0",
                                     false, 0, "integer_ms");
    TCLAP::ValueArg<std::string> configFileArg("", "config", "Path to the openalpr.conf file", false, "",
                                               "config_file");
    TCLAP::ValueArg<std::string> templatePatternArg("p", "pattern",
                                                    "Attempt to match the plate number against a plate pattern (e.g., md for Maryland, ca for California)",
                                                    false, "", "pattern code");
    TCLAP::ValueArg<int> topNArg("n", "topn", "Max number of possible plate numbers to return.  Default=10",
                                 false, 10, "topN");

    TCLAP::SwitchArg jsonSwitch("j", "json", "Output recognition results in JSON format.  Default=off", cmd,
                                false);
    TCLAP::SwitchArg debugSwitch("", "debug", "Enable debug output.  Default=off", cmd, false);
    TCLAP::SwitchArg detectRegionSwitch("d", "detect_region",
                                        "Attempt to detect the region of the plate image.  [Experimental]  Default=off",
                                        cmd, false);
    TCLAP::SwitchArg clockSwitch("", "clock",
                                 "Measure/print the total time to process image and all plates.  Default=off",
                                 cmd, false);
    TCLAP::SwitchArg motiondetect("", "motion", "Use motion detection on video file or stream.  Default=off",
                                  cmd, false);


    try {
        cmd.add(templatePatternArg);
        cmd.add(seekToMsArg);
        cmd.add(topNArg);
        cmd.add(configFileArg);
        cmd.add(fileArg);
        cmd.add(countryCodeArg);

        //filenames = fileArg.getValue();

        country = countryCodeArg.getValue();
        //seektoms = seekToMsArg.getValue();
        outputJson = jsonSwitch.getValue();
        //debug_mode = debugSwitch.getValue();
        configFile = configFileArg.getValue();
        //detectRegion = detectRegionSwitch.getValue();
        templatePattern = templatePatternArg.getValue();
        topn = topNArg.getValue();
        measureProcessingTime = clockSwitch.getValue();
        do_motiondetection = motiondetect.getValue();
    }
    catch (TCLAP::ArgException &e)    // catch any exceptions
    {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        //return 1;
    }

    //Alpr alpr(country, configFile);
    //alpr.setTopN(topn);

    Alpr *alpr[64];

    for(int m = 0; m < 64; m++){
        alpr[m] = new Alpr(country, configFile);
        alpr[m]->setTopN(topn);
    }

    int cont_flag = 0;

    zmq::socket_t *worker_socket[THREAD_NUM];
    for(int worker_count = 0; worker_count < THREAD_NUM; worker_count++){
        worker_socket[worker_count]= new zmq::socket_t(context, ZMQ_PULL);
        worker_socket[worker_count]->bind(raw_pull_address[worker_count].c_str());
    }

    zmq::socket_t *worker_socket3[THREAD_NUM];
    for(int worker_count = 0; worker_count < THREAD_NUM; worker_count++){
        worker_socket3[worker_count]= new zmq::socket_t(context, ZMQ_PULL);
        worker_socket3[worker_count]->bind(raw3_pull_address[worker_count].c_str());
    }

    zmq::socket_t *worker_chunk_socket[THREAD_NUM];
    for(int worker_count = 0; worker_count < THREAD_NUM; worker_count++){
        worker_chunk_socket[worker_count]= new zmq::socket_t(context, ZMQ_PULL);
        worker_chunk_socket[worker_count]->bind(en_pull_address[worker_count].c_str());
        //cout << en_pull_address[worker_count].c_str() << endl;
    }

    k2_measure("alpr begins");

    //while(1){
        Frame f;
        Frame frame[72000];

        request_desc rd;
        vs::sv ser;

        for(int ds = 0; ds < 1; ds++) {
            if(stagePer[ds] == 0.0){
                continue;
            }

            if (stage[ds] == 1){
                rd.db_seq = 2 * (ds % 3);
            }
            else{
                rd.db_seq = 2 * (ds % 3) + 1;
            }
            rd.start_fnum = 0;
            //rd.total_fnum = int(stagePer[ds] * TEMP_NUM[ds / 3]);
            //rd.total_fnum = stoi(argv[1], NULL, 10);

            //rd.total_fnum = stoi(argv[1]);
            rd.total_fnum = 3600;

            cout << stagePer[ds] << " " << TEMP_NUM[ds / 3] << endl;
            rd.id = 0;
            rd.temp = ds / 3;
#if 1
            // Sending requests using service type
            zmq::message_t rq_desc(sizeof(sv));
            ser.type = RETRIEVAL;
            memcpy(&ser.rq, &rd, sizeof(request_desc));
            memcpy(rq_desc.data(), &ser, sizeof(sv));
            s_request.send(rq_desc);

            EE("Request sent, db: %d, frames: %d", ser.rq.db_seq, ser.rq.total_fnum);

            k2_measure("receive begins");
            // Receiving frames
            for (int iter = 0; iter < rd.total_fnum; iter++) {
                data_desc desc;
                shared_ptr<zmq::message_t> msg_ptr;

                //cout << stoi(argv[1], NULL, 10) << endl;

                if(stage[ds] == 1) {
                    msg_ptr = recv_one_frame(*worker_socket[rd.id], &desc);
                }
                    //auto msg_ptr = recv_one_frame(q_request, &desc);
                else{
                    msg_ptr = recv_one_frame(*worker_chunk_socket[rd.id], &desc);
                }

                if (!msg_ptr) {
                    //mg.DepositADesc(desc);
                    frame[count1+iter].msg_p = nullptr;
                    I("got desc: %s", desc.to_string().c_str());
                    if (desc.type == TYPE_CHUNK_EOF || desc.type == TYPE_RAW_FRAME_EOF ||
                        desc.type == TYPE_DECODED_FRAME_EOF) {
                        if (chunk_count == rd.total_fnum/CHUNK_LENGTH - 1) {
                            break;
                        }
                        else{
                            chunk_count ++;
                            continue;
                        }
                    }
                } else {
                    f.desc = desc;
                    f.msg_p = msg_ptr;
                    frame[count1+iter] = f;
                    //mg.DepositAFrame(desc, msg_ptr);
                    //stat.inc_byte_counter((int) msg_ptr->size());
                    //stat.inc_rec_counter(1);
                }
            }
            k2_measure("receive ends");

            zmq::message_t reply(5);
            s_request.recv(&reply);

            count1 += int(stagePer[ds] * TEMP_NUM[ds / 3]);
        }
#endif

        int height_180 = stoi(argv[2], nullptr, 10);
        int width_180 = stoi(argv[3], nullptr, 10);
        int framesize_180 = height_180 * width_180;

        int height_720 = 720;
        int width_720 = 180;
        int framesize_720 = height_720 * width_720;

        int height_ocr = stoi(argv[2], nullptr, 10);
        int width_ocr = stoi(argv[3], nullptr, 10);
        int framesize_ocr = height_ocr * width_ocr;

        Frame fp[64];

        int status_180[50];
        int status_720[50];
        int status_540[50];

        int status_en_180[50];
        int status_en_720[50];
        int status_en_540[50];

        for(int iter = 0; iter < 50; iter ++){
            status_180[iter] = -PREFETCH_BUF-1;
            status_720[iter] = -PREFETCH_BUF-1;
            status_540[iter] = -PREFETCH_BUF-1;
        }

        for(int iter = 0; iter < 50; iter ++){
            status_en_180[iter] = -CHUNK_LENGTH-1;
            status_en_720[iter] = -CHUNK_LENGTH-1;
            status_en_540[iter] = -CHUNK_LENGTH-1;
        }

        //#pragma omp parallel for schedule(dynamic, CHUNK_LENGTH * 6) num_threads(THREAD_NUM)
        #pragma omp parallel for schedule(dynamic, 250) num_threads(1)
        for (int j = 0; j < rd.total_fnum; j++) {
            int id = omp_get_thread_num();
            cv::Mat frm;

            shared_ptr<zmq::message_t> msg_ptr1;
            shared_ptr<zmq::message_t> msg_ptr2;
            shared_ptr<zmq::message_t> msg_ptr3;

            char *imagedata = nullptr;
            if (j % stoi(argv[4], NULL, 10) != 0) continue;


            if(frame[j].msg_p == nullptr) continue;
            imagedata = static_cast<char *>(frame[j].msg_p->data()), frame[j].msg_p->size();

            frm.create(height_180, width_180, CV_8UC1);
            memcpy(frm.data, imagedata, framesize_180);

            std::vector<AlprRegionOfInterest> regionsOfInterest = detectmotion(alpr[id], frm, "", outputJson, id, j, stoi(argv[2], nullptr, 10));

            if (j % stoi(argv[4], NULL, 10) != 0) continue;
            zmq::message_t rq_desc720(sizeof(vs::sv));
            sv ser_720;
            ser_720.type = RETRIEVAL;
            request_desc rd_one_frame;

            if (regionsOfInterest.size() > 0){
                int ds = 0;
                int metric = 0;
                for(int i = 0; i < 1; i ++){
                    metric += stagePer[9+i] * count1;
                    if(j < metric){
                        ds = i;
                        break;
                    }
                }

                //rd_one_frame.start_fnum = j/stoi(argv[4], nullptr, 10);
                rd_one_frame.start_fnum = j;
                if (stage[9+ds] == 1) {
                    rd_one_frame.db_seq = 2 * (ds % 3) + 6;
                    rd_one_frame.total_fnum = PREFETCH_BUF;
                } else {
                    rd_one_frame.db_seq = 2 * (ds % 3) + 7;
                    rd_one_frame.total_fnum = CHUNK_LENGTH;
                }
                rd_one_frame.id = id;
                rd_one_frame.temp = ds/3;

                memcpy(&ser_720.rq, &rd_one_frame, sizeof(request_desc));
                memcpy(rq_desc720.data(), &ser_720, sizeof(sv));

                cv::Mat frm_720;
                Frame f720;

                if (stage[9+ds] == 0) {
                    /********* Initializing 1 CHUNK(250) of Encoded Frames (720p)  **********/
                    if (j - status_en_720[id] >= CHUNK_LENGTH) {
                        service_socket[id]->send(rq_desc720);
                        I("Request sent, start: %d, frame: %d", rd_one_frame.start_fnum, rd_one_frame.total_fnum);
                        zmq::message_t reply(5);
                        service_socket[id]->recv(&reply);

                        for (int iter = 0; iter < rd_one_frame.total_fnum; iter++) {
                            data_desc desc;
                            msg_ptr2 = recv_one_frame(*worker_chunk_socket[id], &desc);
                            pre_enbuf_720[id][iter] = msg_ptr2;

                            if (desc.type == TYPE_CHUNK_EOF || desc.type == TYPE_RAW_FRAME_EOF ||
                                desc.type == TYPE_DECODED_FRAME_EOF) {
                                break;
                            }
                        }
                        status_en_720[id] = j;
                    }
                    msg_ptr2 = pre_enbuf_720[id][j % CHUNK_LENGTH];
                }
                else {
                    /********* Initializing 1 BUFFER(5) of Raw Frames (720p)  **********/
                    if (j - status_720[id] >= PREFETCH_BUF) {
                        service_socket[id]->send(rq_desc720);
                        I("Request sent, start: %d, frame: %d", rd_one_frame.start_fnum, rd_one_frame.total_fnum);
                        zmq::message_t reply(5);
                        service_socket[id]->recv(&reply);

                        for (int iter = 0; iter < rd_one_frame.total_fnum; iter++) {
                            msg_ptr2 = recv_one_chunk_720(*worker_socket[id]);
                            pre_buf_720[id][iter] = msg_ptr2;
                        }
                        status_720[id] = j;
                    }
                    msg_ptr2 = pre_buf_720[id][j % PREFETCH_BUF];
                }

                char *imagedata_720 = NULL;

                if (msg_ptr2 == nullptr) {
                    continue;
                }

                imagedata_720 = static_cast<char *>(msg_ptr2->data()), msg_ptr2->size();

                frm_720.create(height_720, width_720, CV_8UC1);
                memcpy(frm_720.data, imagedata_720, (size_t) framesize_720);
                //cout << frm_720 << endl;

                /* Recognize plate regions */
                queue<PlateRegion> plateQueue = recognizeRegion(alpr[id], frm_720, "", outputJson, id, j,
                                                                regionsOfInterest, stoi(argv[2], nullptr, 10));

                //continue;

                zmq::message_t rq_descocr(sizeof(vs::sv));
                sv ser_540;
                ser_540.type = RETRIEVAL;
                request_desc rd_one_frame_ocr;

                if (plateQueue.size() > 0) {
                    if (j%stoi(argv[4], NULL, 10) != 0) continue;
                    ds = 0;
                    metric = 0;
                    for(int i = 0; i < 1; i ++){
                        metric += stagePer[18+i] * count1;
                        if(j < metric){
                            ds = i;
                            break;
                        }
                    }

                    rd_one_frame_ocr.start_fnum = j/stoi(argv[4], NULL, 10);
                    if (stage[18+ds] == 1) {
                        rd_one_frame_ocr.db_seq = 2 * (ds % 3) + 12;
                        rd_one_frame_ocr.total_fnum = PREFETCH_BUF;
                    } else {
                        rd_one_frame_ocr.db_seq = 2 * (ds % 3) + 13;
                        rd_one_frame_ocr.total_fnum = CHUNK_LENGTH;
                    }
                    rd_one_frame_ocr.id = id;
                    rd_one_frame_ocr.temp = ds/3;

                    memcpy(&ser_540.rq, &rd_one_frame_ocr, sizeof(request_desc));
                    memcpy(rq_descocr.data(), &ser_540, sizeof(sv));

                    cv::Mat frm_ocr;
                    Frame focr;
                    if (stage[18+ds] == 0) {
                        /********** Initializing 5 CHUNKS of New Frames (540p Encoded)  **********/
                        if (j - status_540[id] > CHUNK_LENGTH) {
                            service_socket[id]->send(rq_descocr);
                            I("Request sent, db: %d, frame: %d", rd_one_frame_ocr.db_seq, rd_one_frame_ocr.start_fnum);
                            zmq::message_t reply(5);
                            service_socket[id]->recv(&reply);

                            for (int iter = 0; iter < rd_one_frame_ocr.total_fnum; iter++) {
                                data_desc desc;
                                msg_ptr3 = recv_one_frame(*worker_chunk_socket[id], &desc);
                                pre_enbuf_540[id][iter] = msg_ptr3;

                                if (desc.type == TYPE_CHUNK_EOF || desc.type == TYPE_RAW_FRAME_EOF || desc.type == TYPE_DECODED_FRAME_EOF) {
                                    break;
                                }
                            }
                            status_en_540[id] = j;
                        }
                        msg_ptr3 = pre_enbuf_540[id][j % CHUNK_LENGTH];
                    }
                    else{
                        /********** Initializing 1 BUFFER of New Frames (540p RAW)  **********/
                        if (j - status_540[id] >= PREFETCH_BUF) {
                            service_socket[id]->send(rq_descocr);
                            I("Initial request sent, db: %d, frame: %d", rd_one_frame_ocr.db_seq, rd_one_frame_ocr.start_fnum);
                            zmq::message_t reply(5);
                            service_socket[id]->recv(&reply);

                            for (int iter = 0; iter < rd_one_frame_ocr.total_fnum; iter++) {
                                msg_ptr3 = recv_one_chunk_720(*worker_socket3[id]);
                                data_desc desc;
                                //msg_ptr3 = recv_one_frame(*worker_socket3[id], &desc);
                                pre_buf_540[id][iter] = msg_ptr3;
                            }
                            status_540[id] = j;
                        }
                        msg_ptr3 = pre_buf_540[id][j % PREFETCH_BUF];
                    }

                    char *imagedata_ocr = NULL;

                    if (msg_ptr3 == nullptr) {
                        continue;
                    }
                    imagedata_ocr = (char *) (msg_ptr3->data()), msg_ptr3->size();

                    frm_ocr.create(height_ocr, width_ocr, CV_8UC1);
                    memcpy(frm_ocr.data, imagedata_ocr, framesize_ocr);

                    cv::Mat frm_ocr_gray = frm_ocr;
                    if (frm_ocr.channels() > 2) {
                        cvtColor(frm_ocr, frm_ocr_gray, CV_BGR2GRAY);
                    }

                    getTimeMonotonic(&startTime);
                    bool plate_found = recognizePlate(alpr[id], frm_ocr, frm_ocr_gray, "", outputJson, id, j, plateQueue);
                    getTimeMonotonic(&endTime);
                    std::cout << "Total Plate Recognition: " << diffclock(startTime, endTime) << "ms." << std::endl;

                    if (!plate_found && !outputJson) {
                        std::cout << "No license plates found." << std::endl;
                    }
                }
            }
        }

        k2_measure("alpr ends");
        k2_measure_flush();

        cout << "count1 = " << count1 << endl;
        cout << "count2 = " << count2 << endl;
        cout << "count3 = " << count3 << endl;

    exit(0);
	return 0;
}
