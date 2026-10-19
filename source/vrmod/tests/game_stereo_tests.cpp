#include <evr/game_stereo.hpp>
#include <evr/shadow_projection.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace evr;
int main(int argc, char **argv)
{
    int failed = 0, passed = 0;
    auto check = [&](bool ok, const char *name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
        ok ? ++passed : ++failed;
    };
    game::StableAnchor anchor{{int64_t(1e9 * 4294967296.), 0, 0}, {0, 0, 0, 1}, {0, 1.6f, 0}, 1};
    const auto originalRoot = anchor.root;
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    for (size_t i = 0; i < 2; ++i)
    {
        views[i].pose = {{0, 0, 0, 1}, {i ? .034f : -.031f, .1f, -.2f}};
        views[i].fov = {i ? -.6f : -.9f, i ? .9f : .6f, .8f, -.7f};
    }
    auto make = [&](XrViewStateFlags flags = 3) {
        return game::stereoCameras(anchor, identityPose(), views, flags, 1234567, 42, anchor.root, .01f, 20000);
    };
    const auto pair = make();
    check(pair.xrFrame == 42 && pair.predictedDisplayTime == 1234567, "one frame and predicted time own both eye cameras");
    check(std::abs(relativeQ32(pair.eyes[1].position.x, pair.eyes[0].position.x) - .065) < 1e-7,
          "runtime eye positions preserve asymmetric 65mm baseline at huge coordinates");
    check(std::abs(relativeQ32(pair.eyes[0].position.z, anchor.root.z) - .2) < 1e-7 &&
              std::abs(relativeQ32(pair.eyes[0].position.y, anchor.root.y) - 1.7) < 1e-6,
          "HMD translation applied once and stable height kept separate");
    check(pair.eyes[0].projection.values[8] * pair.eyes[1].projection.values[8] < 0,
          "independent runtime FOVs produce distinct asymmetric projections");
    check(anchor.root.x == originalRoot.x && anchor.root.y == originalRoot.y && anchor.root.z == originalRoot.z,
          "player root remains untouched");
    anchor.worldUnitsPerMeter = 2;
    const auto scaled = make();
    check(std::abs(relativeQ32(scaled.eyes[1].position.x, scaled.eyes[0].position.x) - .13) < 1e-7 &&
              std::abs(relativeQ32(scaled.eyes[0].position.y, anchor.root.y) - 1.8) < 1e-6,
          "world scale affects tracking and IPD, not game-unit root height");
    anchor.worldUnitsPerMeter = 1;
    anchor.orientation = euler(pi / 2, 0, 0);
    const auto turned = make();
    check(std::abs(relativeQ32(turned.eyes[1].position.z, turned.eyes[0].position.z) + .065) < 1e-6,
          "stable anchor facing rotates baseline into game world");
    try { make(0); check(false, "invalid tracking rejected"); }
    catch (const std::invalid_argument &) { check(true, "invalid tracking rejected"); }
    views[1].fov.angleUp = std::numeric_limits<float>::quiet_NaN();
    try { make(); check(false, "invalid right eye prevents publishing stereo pair"); }
    catch (const std::invalid_argument &) { check(true, "invalid right eye prevents publishing stereo pair"); }
    views[1].fov.angleUp = .8f;
    anchor.orientation = {0, 0, 0, 1};
    TrackingOrigin tracking;
    const Pose head{euler(pi / 2, 0, 0), {.3f, 1.7f, -.4f}};
    tracking.recenter(head);
    for (int eye = 0; eye < 2; ++eye)
        views[eye].pose = compose(head, Pose{euler(eye ? .04f : -.03f, 0, 0), {eye ? .034f : -.031f, 0, 0}});
    auto centered = game::stereoCameras(anchor, tracking.trackingToAnchor, views, 3, 100, 1, anchor.root, .01f, 20000);
    check(std::abs(relativeQ32(centered.eyes[0].position.y, anchor.root.y) - 1.6) < 1e-6 &&
              std::abs(relativeQ32(centered.eyes[1].position.x, anchor.root.x) - .034) < 1e-6,
          "recenter removes tracked origin while preserving IPD and calibrated root height");
    check(length(rotate(centered.eyes[0].orientation, {0, 0, 1}) -
                 rotate(centered.eyes[1].orientation, {0, 0, 1})) > .06f,
          "runtime canted eye rotations remain independent");
    // A world point back into the XR space (trackingPointFromWorld): each eye's own world position gives its XR position,
    // for a turned anchor, a camera offset, a tracking origin away from identity and two world scales.
    for(const double units:{1.0,2.0}){
        game::StableAnchor turnedAnchor=anchor;turnedAnchor.orientation=euler(.7f,-.15f,.05f);turnedAnchor.worldUnitsPerMeter=units;
        const Pose origin{euler(-1.1f,.1f,.02f),{.4f,-1.2f,.9f}};
        const auto cams=game::stereoCameras(turnedAnchor,origin,views,3,100,1,turnedAnchor.root,.01f,10000);
        for(unsigned eye=0;eye<2;++eye){
            const auto back=game::trackingPointFromWorld(turnedAnchor,origin,cams.eyes[eye].position);
            check(length(back-views[eye].pose.position)<2e-5f,"world point maps back to the eye's XR position");
        }
        // and a point 3 m ahead of the left eye along its view
        const Vec3 ahead=views[0].pose.position+rotate(views[0].pose.orientation,{0,0,-3});
        const auto local=game::trackingPose(compose(origin,Pose{{0,0,0,1},ahead}));
        const auto world=cameraPosition(cameraPosition(turnedAnchor.root,rotate(normalized(turnedAnchor.orientation),turnedAnchor.cameraOffsetGameUnits),1),
            rotate(normalized(turnedAnchor.orientation),local.position),units);
        check(length(game::trackingPointFromWorld(turnedAnchor,origin,world)-ahead)<5e-5f,"a world point ahead maps to its XR point");
    }
    // Same captured world cameras across independently recentered sessions.
    game::StableAnchor reference=anchor;reference.cameraOffsetGameUnits={0,0,0};reference.orientation=euler(.8f,.1f,-.04f);
    const auto expectedReplay=game::stereoCameras(reference,identityPose(),views,3,100,1,reference.root,.01f,10000);
    for(const auto yaw:{-.9f,0.f,1.8f}) {
        const Pose recenter{euler(yaw,.2f,-.1f),{.7f,-1.4f,.2f}};
        const auto replay=game::replayAnchor(reference,recenter);
        const auto cameras=game::stereoCameras(replay,recenter,views,3,100,1,reference.root,.01f,10000);
        for(unsigned eye=0;eye<2;++eye){const auto& a=cameras.eyes[eye];const auto& b=expectedReplay.eyes[eye];
            check(std::abs(relativeQ32(a.position.x,b.position.x))<2e-6&&std::abs(relativeQ32(a.position.y,b.position.y))<2e-6&&
                std::abs(relativeQ32(a.position.z,b.position.z))<2e-6,"draw replay keeps world position after recenter");
            check(length(rotate(a.orientation,{0,0,1})-rotate(b.orientation,{0,0,1}))<2e-6,"draw replay keeps world rays after recenter");
        }
    }
    for(auto fov:{XrFovf{-.95f,.95f,.93f,-.93f},XrFovf{-.95f,.74f,.77f,-.93f}}){
        const auto eye=game::projectionDescriptor(fov,.01f,10000);const auto preserved=eye;
        const auto shadow=game::shadowPerspectiveDescriptor(eye);
        uint32_t tag=99;std::memcpy(&tag,shadow.bytes.data(),4);
        std::array<float,4> fields{};std::memcpy(fields.data(),shadow.bytes.data()+4,16);
        const float x=std::max(-std::tan(fov.angleLeft),std::tan(fov.angleRight));
        const float y=std::max(-std::tan(fov.angleDown),std::tan(fov.angleUp));
        check(tag==0&&std::abs(fields[0]-x/y)<1e-6&&fields[1]==.01f&&fields[2]==10000&&std::abs(fields[3]-2*std::atan(y))<1e-6,
              "native shadow fields carry aspect/clip/FOV and contain both eye sides");
        check(eye.bytes==preserved.bytes&&shadow.bytes[0x48]==1,"shadow conversion preserves source and reverse depth");
        for(unsigned kind=0;kind<5;kind++){
            auto invalid=eye;
            if(kind==0)invalid.bytes[0]=0;
            if(kind==1)invalid.bytes[0x44]=1;
            if(kind==2)invalid.bytes[0x48]=0;
            if(kind==3){float v=std::numeric_limits<float>::quiet_NaN();std::memcpy(invalid.bytes.data()+4,&v,4);}
            if(kind==4){float v=0;std::memcpy(invalid.bytes.data()+0x14,&v,4);}
            try{game::shadowPerspectiveDescriptor(invalid);check(false,"invalid shadow input rejected");}
            catch(const std::invalid_argument&){check(true,"invalid shadow input rejected");}
        }
    }
    if (argc == 3 && std::string(argv[1]) == "--vectors")
    {
        std::ofstream out(argv[2]);
        out << std::setprecision(10) << '[';
        bool first = true;
        for (int sample = 0; sample < 32; ++sample)
        {
            const auto head = Pose{euler(sample * .2f, std::sin(float(sample)) * 1.5f, sample * .1f),
                                   {std::sin(float(sample)) * .2f, .1f, std::cos(float(sample)) * .15f}};
            for (int eye = 0; eye < 2; ++eye)
            {
                views[eye].pose = compose(head, Pose{{0, 0, 0, 1}, {eye ? .034f : -.031f, 0, 0}});
                views[eye].fov = {-.9f + sample * .005f, .6f + eye * .2f,
                                 .8f - sample * .004f, -.7f - eye * .1f};
            }
            auto cameras = make();
            for (const auto &eye : cameras.eyes)
            {
                if (!first) out << ',';
                first = false;
                out << "{\"descriptor\":\"";
                const char *hex = "0123456789abcdef";
                for (auto b : eye.descriptor.bytes) out << hex[b >> 4] << hex[b & 15];
                out << "\",\"projection\":[";
                for (size_t j = 0; j < 16; ++j) { if (j) out << ','; out << eye.projection.values[j]; }
                out << "]}";
                const auto shadow=game::shadowPerspectiveDescriptor(eye.descriptor);
                const float x=std::max(-eye.fov.angleLeft,eye.fov.angleRight),y=std::max(-eye.fov.angleDown,eye.fov.angleUp);
                const auto expected=game::projection({-x,x,y,-y},.01f,20000);
                out << ",{\"descriptor\":\"";
                for(auto b:shadow.bytes)out<<hex[b>>4]<<hex[b&15];
                out << "\",\"projection\":[";
                for(size_t j=0;j<16;j++){if(j)out<<',';out<<expected.values[j];}out<<"]}";
            }
        }
        out << "]\n";
        if (!out) return 2;
    }
    std::cout << passed << " PASS / " << failed << " FAIL\n";
    return failed ? 1 : 0;
}
