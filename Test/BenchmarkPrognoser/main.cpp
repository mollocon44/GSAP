//
//  progMain.cpp
//  Example
//
//  Created by Teubert, Christopher (ARC-TI)[SGT, INC] on 5/4/16.
//  Copyright © 2018 United States Government as represented by the Administrator of the National Aeronautics and Space Administration.  All Rights Reserved.
//

#include "ProgManager.h"

#include "PrognoserFactory.h"
#include "BenchmarkPrognoser.h"

#include "CommunicatorFactory.h"
#include "RandomCommunicator.h"
#include "RecorderCommunicator.h"
#include "PlaybackCommunicator.h"
#include "ConfigMap.h"
#include "ModelFactory.h"
#include "PrognosticsModelFactory.h"
#include "ObserverFactory.h"
#include "PredictorFactory.h"
#include "UnscentedKalmanFilter.h"
#include "MonteCarloPredictor.h"
#include "Battery.h"
#include <ctime>
#include <fstream>
#include <iostream>
//#include "BenchmarkTest.cpp"

using namespace PCOE;

int main() {
    ConfigMap::addSearchPath("../example/cfg/");

    // Specify Prognosers
    PrognoserFactory & prognoserFactory = PrognoserFactory::instance();
    prognoserFactory.Register("BenchmarkPrognoser", PrognoserFactory::Create<BenchmarkPrognoser>);

    // Specify Communicators
    CommunicatorFactory & commFactory = CommunicatorFactory::instance();
    commFactory.Register("recorder",  CommunicatorFactory::Create<RecorderCommunicator>);
    commFactory.Register("playback",  CommunicatorFactory::Create<PlaybackCommunicator>);

    // Register battery model
    ModelFactory & pModelFactory = ModelFactory::instance();
    PrognosticsModelFactory & pProgModelFactory = PrognosticsModelFactory::instance();
    pModelFactory.Register("Battery", ModelFactory::Create<Battery>);
    pProgModelFactory.Register("Battery", PrognosticsModelFactory::Create<Battery>);

    // Register UKF
    ObserverFactory & pObserverFactory = ObserverFactory::instance();
    pObserverFactory.Register("UKF", ObserverFactory::Create<UnscentedKalmanFilter>);

    // Register MonteCarloPredictor
    PredictorFactory & pPredictorFactory = PredictorFactory::instance();
    pPredictorFactory.Register("MC", PredictorFactory::Create<MonteCarloPredictor>);

    ProgManager PM = ProgManager("bench.cfg");
    
    printf("Benchmarking\n");
    PM.enable();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    PM.start();
    std::this_thread::sleep_for(std::chrono::minutes(1));
    PM.stop();
  
    return 0;
}
