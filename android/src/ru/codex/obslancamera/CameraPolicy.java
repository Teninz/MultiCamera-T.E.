package ru.codex.obslancamera;
/** Pure selection rules used by Camera2 controls and JVM regression checks. */
final class CameraPolicy {
    static boolean has(int[] values,int wanted){if(values!=null)for(int value:values)if(value==wanted)return true;return false;}
    // Stabilization plan: preview EIS, video EIS, OIS, off. Never enable EIS and OIS together.
    static int[] stabilizationPlans(int[] video,int[] optical,int width,int height,int fps){
        java.util.ArrayList<Integer> plans=new java.util.ArrayList<>();
        boolean demanding=width>1920||height>1080||fps>30;
        if(demanding&&has(optical,1))plans.add(3);
        if(has(video,2))plans.add(2);
        if(has(video,1))plans.add(1);
        if(!demanding&&has(optical,1))plans.add(3);
        plans.add(0);int[] result=new int[plans.size()];for(int i=0;i<result.length;i++)result[i]=plans.get(i);return result;
    }
    static boolean fpsInRanges(int fps,int[][] ranges){if(ranges==null)return fps<=30;for(int[] range:ranges)if(range[0]<=fps&&fps<=range[1])return true;return false;}
    static final class Stabilizer {
        private final int[] plans;private int index,frames;private boolean confirmed;private String actual="Стаб. проверка…";
        Stabilizer(int[] plans){this.plans=plans;}
        int videoMode(){int plan=plans[index];return plan==1||plan==2?plan:0;}
        int opticalMode(){return plans[index]==3?1:0;}
        String requested(){return label(videoMode(),opticalMode());}
        String actual(){return actual;}
        private static String label(int video,int optical){return video==2?"Стаб. EIS превью":video==1?"Стаб. EIS":optical==1?"Стаб. OIS":"Стаб. выкл";}
        boolean observe(Integer video,Integer optical,int targetFps){
            if(video==null&&optical==null){actual=plans.length==1?"Стаб. недоступна":"Стаб. не подтверждена";return false;}
            int v=video==null?0:video,o=optical==null?0:optical;
            if(v!=0||o!=0){actual=label(v,o);confirmed=true;frames=0;return false;}
            if(confirmed){confirmed=false;frames=0;}
            if(plans[index]==0){actual=plans.length==1?"Стаб. недоступна":"Стаб. нет в этом режиме";return false;}
            if(++frames<Math.max(30,targetFps))return false;
            if(index+1<plans.length){index++;frames=0;actual="Стаб. проверка…";return true;}return false;
        }
    }
}
