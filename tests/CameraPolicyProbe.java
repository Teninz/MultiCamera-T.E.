package ru.codex.obslancamera;
import java.util.Arrays;
public class CameraPolicyProbe {
 static void check(boolean value){if(!value)throw new AssertionError();}
 static void plans(int[] got,int... expected){if(!Arrays.equals(got,expected))throw new AssertionError(Arrays.toString(got));}
 public static void main(String[] args){
  plans(CameraPolicy.stabilizationPlans(new int[]{0},new int[]{0},1280,720,30),0);
  plans(CameraPolicy.stabilizationPlans(null,null,1280,720,30),0);
  plans(CameraPolicy.stabilizationPlans(new int[]{0,1},new int[]{0},1280,720,30),1,0);
  plans(CameraPolicy.stabilizationPlans(new int[]{0},new int[]{0,1},1280,720,60),3,0);
  plans(CameraPolicy.stabilizationPlans(new int[]{0,1,2},new int[]{0,1},1920,1080,30),2,1,3,0);
  plans(CameraPolicy.stabilizationPlans(new int[]{0,1,2},new int[]{0,1},1920,1080,60),3,2,1,0);
  plans(CameraPolicy.stabilizationPlans(new int[]{0,1},new int[]{0,1},3840,2160,30),3,1,0);
  check(!CameraPolicy.fpsInRanges(60,new int[][]{{15,30},{30,30}}));
  check(CameraPolicy.fpsInRanges(60,new int[][]{{15,30},{30,60},{60,60}}));
  check(!CameraPolicy.fpsInRanges(60,null));check(CameraPolicy.fpsInRanges(30,null));
  CameraPolicy.Stabilizer state=new CameraPolicy.Stabilizer(new int[]{2,1,3,0});
  check(state.videoMode()==2&&state.opticalMode()==0);
  for(int frame=0;frame<29;frame++)check(!state.observe(0,0,30));
  check(state.observe(0,0,30));check(state.videoMode()==1&&state.opticalMode()==0);
  for(int frame=0;frame<100;frame++)check(!state.observe(1,0,30));check(state.actual().equals("Стаб. EIS"));
  for(int frame=0;frame<29;frame++)check(!state.observe(0,0,30));check(state.observe(0,0,30));check(state.videoMode()==0&&state.opticalMode()==1);
  check(!state.observe(0,1,30));check(state.actual().equals("Стаб. OIS"));
  for(int frame=0;frame<59;frame++)check(!state.observe(0,0,60));check(state.observe(0,0,60));check(state.videoMode()==0&&state.opticalMode()==0);state.observe(0,0,60);check(state.actual().equals("Стаб. нет в этом режиме"));
  CameraPolicy.Stabilizer unavailable=new CameraPolicy.Stabilizer(new int[]{0});unavailable.observe(null,null,30);check(unavailable.actual().equals("Стаб. недоступна"));
  CameraPolicy.Stabilizer unknown=new CameraPolicy.Stabilizer(new int[]{1,0});unknown.observe(null,null,30);check(unknown.actual().equals("Стаб. не подтверждена"));
  System.out.println("CAMERA_POLICY_PASS no stabilization, EIS, preview EIS, OIS, 60fps priority, no unsupported 60fps");
 }
}
