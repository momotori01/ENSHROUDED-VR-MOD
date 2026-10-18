#include <cstdlib>
#include <cstring>
#include <iostream>
#include <openvr.h>
int main(int argc, char **argv)
{
    if (argc > 1 && (!std::strcmp(argv[1], "configure-test") || !std::strcmp(argv[1], "calibrate-test") ||
                     !std::strcmp(argv[1], "quit-compositor") || !std::strcmp(argv[1], "recenter-test")))
    {
        const char *cfg = std::getenv("VR_CONFIG_PATH");
        if (!cfg || std::string(cfg).find("test_env") == std::string::npos)
        {
            std::cerr << "Refusing mutation outside isolated test_env\n";
            return 2;
        }
    }
    vr::EVRInitError error;
    auto system = vr::VR_Init(&error, vr::VRApplication_Utility);
    if (error != vr::VRInitError_None)
    {
        std::cerr << vr::VR_GetVRInitErrorAsEnglishDescription(error) << '\n';
        return 1;
    }
    if(argc>1) {
        char serial[256]{};
        vr::ETrackedPropertyError propertyError;
        system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,vr::Prop_SerialNumber_String,serial,sizeof(serial),&propertyError);
        if(propertyError!=vr::TrackedProp_Success || (std::strcmp(serial,"EVR-VIRTUAL-001") && std::strcmp(serial,"EVR-Test-Null"))) {
            std::cerr<<"Refusing mutation: active HMD is not an EVR test device\n";vr::VR_Shutdown();return 2;
        }
    }
    if (argc > 1 && !std::strcmp(argv[1], "configure-test"))
    {
        vr::VRSettings()->SetBool("power", "pauseCompositorOnStandby", false);
        vr::VRSettings()->SetFloat("power", "turnOffScreensTimeout", 0);
        vr::VRSettings()->SetBool("dashboard", "enableDashboard", false);
        std::cout << "PASS isolated SteamVR test power settings\n";
    }
    else if (argc > 1 && !std::strcmp(argv[1], "calibrate-test"))
    {
        const char *cfg = std::getenv("VR_CONFIG_PATH");
        if (!cfg || std::string(cfg).find("test_env") == std::string::npos)
        {
            std::cerr << "Refusing calibration outside isolated test_env\n";
            vr::VR_Shutdown();
            return 2;
        }
        vr::HmdMatrix34_t identity{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
        auto setup = vr::VRChaperoneSetup();
        setup->RevertWorkingCopy();
        setup->SetWorkingStandingZeroPoseToRawTrackingPose(&identity);
        setup->SetWorkingSeatedZeroPoseToRawTrackingPose(&identity);
        setup->SetWorkingPlayAreaSize(3, 3);
        if (!setup->CommitWorkingCopy(vr::EChaperoneConfigFile_Live))
        {
            std::cerr << "calibration failed\n";
            vr::VR_Shutdown();
            return 1;
        }
        std::cout << "PASS isolated identity tracking origin\n";
    }
    else if(argc>1&&!std::strcmp(argv[1],"recenter-test")) {
        vr::VRChaperone()->ResetZeroPose(vr::TrackingUniverseSeated);
        std::cout<<"PASS recenter requested for isolated test HMD\n";
    }
    else if (argc > 1 && !std::strcmp(argv[1], "quit-compositor"))
    {
        vr::VRCompositor()->CompositorQuit();
    }
    else
    {
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
        system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseRawAndUncalibrated, 0, poses,
                                                vr::k_unMaxTrackedDeviceCount);
        std::cout << "connected=" << poses[0].bDeviceIsConnected << " valid=" << poses[0].bPoseIsValid
                  << " xyz=" << poses[0].mDeviceToAbsoluteTracking.m[0][3] << ','
                  << poses[0].mDeviceToAbsoluteTracking.m[1][3] << ',' << poses[0].mDeviceToAbsoluteTracking.m[2][3]
                  << '\n';
    }
    vr::VR_Shutdown();
    return 0;
}
