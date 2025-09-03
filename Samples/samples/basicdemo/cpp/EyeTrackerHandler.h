//
// Created by user on 03-Sep-25.
//

#include "openxr/openxr.h"
#include "BasicOpenXrWrapper.h"
#include <sstream>  // for std::ostringstream

#ifndef PICONATIVEOPENXRSAMPLES_EYETRACKERHANDLER_H
#define PICONATIVEOPENXRSAMPLES_EYETRACKERHANDLER_H

#define CHK_XR(x) \
    do { \
        XrResult res = x; \
        if (XR_FAILED(res)) { \
            PLOGE("[DEBUGGING] OpenXR call failed: %d\n", res); \
            exit(1); \
        } \
    } while(0)


#pragma pack(push, 1)
struct EtData {
    float leftEyeOpenness;
    float rightEyeOpenness;

    float leftEyePupilDilation;
    float rightEyePupilDilation;

    float leftEyeMiddleCanthusUvX;
    float leftEyeMiddleCanthusUvY;
    float rightEyeMiddleCanthusUvX;
    float rightEyeMiddleCanthusUvY;
};
#pragma pack(pop)

class EyeTrackerHandler {
private:
    static PFN_xrCreateEyeTrackerPICO xrCreateEyeTrackerPICO;
    static PFN_xrDestroyEyeTrackerPICO xrDestroyEyeTrackerPICO;
    static PFN_xrGetEyeDataPICO xrGetEyeDataPICO;
    static XrEyeTrackerPICO eyeTracker;
    static bool isInitialized;

public:
    // EyeTrackerHandler();
    static void Initialize(PVRSampleFW::BasicOpenXrWrapper *openxr_wrapper) {
        if (isInitialized) return; isInitialized = true;

        auto instance = openxr_wrapper->GetXrInstance();
        auto session = openxr_wrapper->GetXrSession();

        CHK_XR(xrGetInstanceProcAddr(instance, "xrCreateEyeTrackerPICO",
                              reinterpret_cast<PFN_xrVoidFunction*>(
                                      &xrCreateEyeTrackerPICO)));

        CHK_XR(xrGetInstanceProcAddr(instance, "xrDestroyEyeTrackerPICO",
                              reinterpret_cast<PFN_xrVoidFunction*>(
                                      &xrDestroyEyeTrackerPICO)));

        CHK_XR(xrGetInstanceProcAddr(instance, "xrGetEyeDataPICO",
                              reinterpret_cast<PFN_xrVoidFunction*>(
                                      &xrGetEyeDataPICO)));

        XrEyeTrackerCreateInfoPICO createInfo{XR_TYPE_EYE_TRACKER_CREATE_INFO_PICO};
        createInfo.next = nullptr;

        CHK_XR(xrCreateEyeTrackerPICO(session, &createInfo, &eyeTracker));
    }


    static void LogEyeData(const XrEyeTrackerDataPICO& eyeData) {
        std::ostringstream oss;

        oss << "Left Eye: openness=" << eyeData.leftEyeData.openness
            << ", pupilDilation=" << eyeData.leftEyeData.pupilDilation
            << ", middleCanthusUv=(" << eyeData.leftEyeData.middleCanthusUv.x
            << ", " << eyeData.leftEyeData.middleCanthusUv.y << ") | "
            << "Right Eye: openness=" << eyeData.rightEyeData.openness
            << ", pupilDilation=" << eyeData.rightEyeData.pupilDilation
            << ", middleCanthusUv=(" << eyeData.rightEyeData.middleCanthusUv.x
            << ", " << eyeData.rightEyeData.middleCanthusUv.y << ")";

        PLOGI(oss.str().c_str());  // convert to C-style string for logging
    }


    static EtData ProcessData(PVRSampleFW::BasicOpenXrWrapper *openxr_wrapper) {
        Initialize(openxr_wrapper);

        XrEyeTrackerDataPICO eyeData = {XR_TYPE_EYE_TRACKER_DATA_PICO, nullptr};
        XrEyeTrackerDataInfoPICO eyeDataInfo = {
                XR_TYPE_EYE_TRACKER_DATA_INFO_PICO,
                nullptr,
                XR_EYE_TRACKER_LEFT_BIT_PICO | XR_EYE_TRACKER_RIGHT_BIT_PICO,
        };
        CHK_XR(xrGetEyeDataPICO(eyeTracker, &eyeDataInfo, &eyeData));
        LogEyeData(eyeData);

        EtData dataToExport = {};

        dataToExport.leftEyeOpenness = eyeData.leftEyeData.openness;
        dataToExport.rightEyeOpenness = eyeData.rightEyeData.openness;

        dataToExport.leftEyePupilDilation = eyeData.leftEyeData.pupilDilation;
        dataToExport.rightEyePupilDilation = eyeData.rightEyeData.pupilDilation;

        dataToExport.leftEyeMiddleCanthusUvX = eyeData.leftEyeData.middleCanthusUv.x;
        dataToExport.leftEyeMiddleCanthusUvY = eyeData.leftEyeData.middleCanthusUv.y;

        dataToExport.rightEyeMiddleCanthusUvX = eyeData.rightEyeData.middleCanthusUv.x;
        dataToExport.rightEyeMiddleCanthusUvY = eyeData.rightEyeData.middleCanthusUv.y;

        // PLOGI("[ETDATA]");
        // PLOGI(dataToExport.leftEyeOpenness);


        return dataToExport;
    }

    static void DisposeTracker() {
        CHK_XR(xrDestroyEyeTrackerPICO(eyeTracker));
    }
};
PFN_xrCreateEyeTrackerPICO EyeTrackerHandler::xrCreateEyeTrackerPICO = XR_NULL_HANDLE;
PFN_xrDestroyEyeTrackerPICO EyeTrackerHandler::xrDestroyEyeTrackerPICO = XR_NULL_HANDLE;
PFN_xrGetEyeDataPICO EyeTrackerHandler::xrGetEyeDataPICO = XR_NULL_HANDLE;
XrEyeTrackerPICO EyeTrackerHandler::eyeTracker = XR_NULL_HANDLE;
bool EyeTrackerHandler::isInitialized = false;
#endif //PICONATIVEOPENXRSAMPLES_EYETRACKERHANDLER_H
