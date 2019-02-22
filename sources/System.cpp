#include "System.h"
#include "Tracking.h"
#include "PoseEstimation.h"
namespace Monocular {
    System::System(const Camera &cam,eTrackingMode mode)
    {
        mpTracker       =  new Tracking(static_cast<eFrameType>((int)mode));
        mpEstimation    =  new PoseEstimation(cam);
    }
    
    System::~System()
    {
        DESTROYOBJECT(mpTracker);
        DESTROYOBJECT(mpEstimation);
    }
    
    //检测框大小
    inline bool checkRect(const Rect2f &rect)
    {
        return (rect.height > MINOBJECTSIZE) && (rect.width > MINOBJECTSIZE);
    }
    
    TargetItems System::objectDetect(const cv::Mat &img)
    {
        //add more ...
        static int index = 0;
        TargetItems items;
        const float sz = 50.0f;
        if( index++ < 1 )
        {
            Point2f pt(2756,866);
            TargetItem item{0,pt,Rect2f(pt.x - sz,pt.y - sz,sz * 2,sz * 2)};
            
#ifdef TESTOUTPUT
            item._realpos = GeoPos(114.40350395, 30.60123760);
#endif
            if(checkRect(item._box))
                items.emplace_back(item);
            else
                assert(NULL);
        }
        else
        {
            Point2f pt(2907,827);
            TargetItem item{0,pt,Rect2f(pt.x - sz,pt.y - sz,sz * 2,sz * 2)};
            
#ifdef TESTOUTPUT
            item._realpos = GeoPos(114.40350395, 30.60123760);
#endif
             if(checkRect(item._box))
                 items.emplace_back(item);
            else
                assert(NULL);
        }
        
        
        return items;
    }
    
    
    void System::handle(const cv::Mat &img,const GeoPos &geopt,const TargetItems &items)
    {
        if( NULL != mpTracker )
        {
            //add more ..
            
            if(mpTracker->grabImage(img,geopt,items))
            {//跟踪成功,即匹配对有值
                assert(mpEstimation);
                PtVector prepts,curpts;
                mpTracker->getMatchVector(prepts,curpts);
                mpEstimation->estimate(mpTracker->getFrame(ePreFrame), mpTracker->getFrame(eCurFrame), prepts, curpts);
            }
            
            //add more ..
        }
    }
    
    void System::reset()
    {
        if(NULL != mpTracker)mpTracker->reset();
    }
}