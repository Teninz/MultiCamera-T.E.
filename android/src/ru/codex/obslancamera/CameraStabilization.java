package ru.codex.obslancamera;
import android.hardware.camera2.*;
/** Requests supported stabilization, then checks actual CaptureResult modes. */
final class CameraStabilization {
    private final CameraPolicy.Stabilizer state;
    CameraStabilization(CameraCharacteristics caps,int width,int height,int fps){state=new CameraPolicy.Stabilizer(CameraPolicy.stabilizationPlans(caps.get(CameraCharacteristics.CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES),caps.get(CameraCharacteristics.LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION),width,height,fps));}
    void apply(CaptureRequest.Builder request){request.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE,state.videoMode());request.set(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE,state.opticalMode());}
    String requested(){return state.requested();}
    String actual(){return state.actual();}
    boolean result(TotalCaptureResult result,int targetFps){return state.observe(result.get(CaptureResult.CONTROL_VIDEO_STABILIZATION_MODE),result.get(CaptureResult.LENS_OPTICAL_STABILIZATION_MODE),targetFps);}
    static String capabilities(CameraCharacteristics caps){if(caps==null)return Language.text("Возможности камеры ещё не определены");int[] v=caps.get(CameraCharacteristics.CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES),o=caps.get(CameraCharacteristics.LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION);return "EIS="+CameraPolicy.has(v,1)+", preview EIS="+CameraPolicy.has(v,2)+", OIS="+CameraPolicy.has(o,1);}
}
