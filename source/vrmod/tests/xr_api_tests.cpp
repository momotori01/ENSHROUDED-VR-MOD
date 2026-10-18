#include <evr/openxr_client.hpp>
#include <filesystem>
#include <iostream>
using namespace evr;
int failed = 0, passed = 0;
void expect(bool ok, const char *name)
{
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    (ok ? passed : failed)++;
}
int main()
{
    std::filesystem::create_directories("api-test-output");
    Log log("api-test-output/client.jsonl", true);
    try
    {
        OpenXrClient xr(log);
        xr.init(1);
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        expect(xrBeginFrame(xr.session, &begin) == XR_ERROR_SESSION_NOT_RUNNING, "begin frame before session rejected");
        xr.poll();
        expect(xr.active(), "READY begins session");
        expect(xrBeginFrame(xr.session, &begin) == XR_ERROR_CALL_ORDER_INVALID, "begin without wait rejected");
        XrReferenceSpaceCreateInfo ref{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        ref.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        ref.poseInReferenceSpace = identityPose();
        ref.poseInReferenceSpace.position = {.25f, .1f, -.2f};
        XrSpace shifted;
        xrCheck(xrCreateReferenceSpace(xr.session, &ref, &shifted), "shifted origin");
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        xrCheck(xrLocateSpace(shifted, xr.local, 1, &loc), "locate shifted origin");
        expect(length(loc.pose.position - ref.poseInReferenceSpace.position) < 1e-6f,
               "reference-space offset applied once");
        xrDestroySpace(shifted);
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs{XR_TYPE_FRAME_STATE};
        xrCheck(xrWaitFrame(xr.session, &wi, &fs), "wait frame");
        xrCheck(xrBeginFrame(xr.session, &begin), "begin frame");
        XrViewLocateInfo vi{XR_TYPE_VIEW_LOCATE_INFO};
        vi.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        vi.displayTime = fs.predictedDisplayTime;
        vi.space = xr.local;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
        uint32_t count;
        expect(xrLocateViews(xr.session, &vi, &vs, 1, &count, views) == XR_ERROR_SIZE_INSUFFICIENT && count == 2,
               "view enumeration capacity checked");
        xrCheck(xrLocateViews(xr.session, &vi, &vs, 2, &count, views), "locate views");
        expect(length(views[1].pose.position - views[0].pose.position) > .02f, "runtime supplies distinct eye poses");
        auto swap = xr.targets[0].handle;
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        XrSwapchainImageWaitInfo swi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        swi.timeout = 1000000;
        uint32_t index;
        xrCheck(xrAcquireSwapchainImage(swap, &ai, &index), "acquire");
        expect(xrAcquireSwapchainImage(swap, &ai, &index) == XR_ERROR_CALL_ORDER_INVALID,
               "duplicate acquire rejected by test runtime");
        expect(xrReleaseSwapchainImage(swap, &ri) == XR_ERROR_CALL_ORDER_INVALID, "release without wait rejected");
        xrCheck(xrWaitSwapchainImage(swap, &swi), "wait image");
        xrCheck(xrReleaseSwapchainImage(swap, &ri), "release image");
        expect(xrReleaseSwapchainImage(swap, &ri) == XR_ERROR_CALL_ORDER_INVALID, "duplicate release rejected");
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = fs.predictedDisplayTime;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        xrCheck(xrEndFrame(xr.session, &end), "end frame zero layers");
        expect(xrEndFrame(xr.session, &end) == XR_ERROR_CALL_ORDER_INVALID, "duplicate end frame rejected");
        bool threw = false;
        try
        {
            xr.frame([](uint32_t, uint32_t, const XrView &, XrTime, uint64_t) {
                throw std::runtime_error("injected render failure");
            });
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }
        expect(threw, "render exception propagates");
        expect(xr.frame([](uint32_t, uint32_t, const XrView &, XrTime, uint64_t) {}),
               "frame protocol recovers after render exception");
        // No-op callback validates protocol recovery only, and is not image-render evidence.
        expect(xr.beginFrame(), "staged frame begins and returns before submission");
        const auto staged = xr.pendingFrame();
        try { xr.beginFrame(); expect(false, "duplicate staged begin rejected"); }
        catch (const std::logic_error &) { expect(true, "duplicate staged begin rejected"); }
        expect(xr.pendingFrame().frame == staged.frame && xr.pendingFrame().predictedDisplayTime == staged.predictedDisplayTime,
               "pending pose snapshot/time remain stable across render phases");
        try { xr.markEyeRendered(0, staged.frame - 1); expect(false, "stale completion rejected"); }
        catch (const std::logic_error &) { expect(true, "stale completion rejected"); }
        xr.markEyeRendered(0, staged.frame); // protocol fixture only; no GPU evidence
        try { xr.markEyeRendered(0, staged.frame); expect(false, "duplicate eye completion rejected"); }
        catch (const std::logic_error &) { expect(true, "duplicate eye completion rejected"); }
        try { xr.finishFrame(); expect(false, "incomplete pair rejected"); }
        catch (const std::logic_error &) { expect(!xr.hasPendingFrame(), "incomplete pair rejected and pending state cleared"); }
        expect(xr.beginFrame(), "staged protocol recovers after incomplete pair");
        xr.markEyeRendered(0, xr.pendingFrame().frame);
        xr.markEyeRendered(1, xr.pendingFrame().frame);
        xr.finishFrame();
        expect(!xr.hasPendingFrame(), "completed pair clears staged frame");
        try { xr.finishFrame(); expect(false, "duplicate staged end rejected"); }
        catch (const std::logic_error &) { expect(true, "duplicate staged end rejected"); }
        expect(xr.beginFrame(), "pending frame created for teardown test");
        xr.shutdown();
        expect(!xr.hasPendingFrame(), "shutdown aborts pending frame before destroying swapchains");
        expect(xr.instance == XR_NULL_HANDLE && xr.session == XR_NULL_HANDLE, "session teardown clears ownership");
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL exception " << e.what() << '\n';
        failed++;
    }
    std::cout << "RESULT " << passed << " PASS " << failed << " FAIL (mock protocol checks)\n";
    return failed ? 1 : 0;
}
