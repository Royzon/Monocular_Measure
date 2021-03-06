#include "PoseEstimation.h"
#include "CvHeader.h"
#include "Functions.h"
#include "Frame.h"
#include "Serialization.h"
namespace Monocular {
    
    
    void PoseEstimation::estimateF_2d2d(const KeyPointVector &kypt1, const KeyPointVector &kypt2, const MatchVector &matches, cv::Mat &R, cv::Mat &t)
    {
        PtVector points1;
        PtVector points2;
        
        for(int i = 0;i < (int)matches.size(); ++i)
        {
            points1.push_back( kypt1[matches[i].queryIdx].pt);
            points2.push_back( kypt2[matches[i].trainIdx].pt);
        }
        
        estimateF_2d2d(points1, points2, R, t);
    }
    
    
    typedef void (PoseEstimation::*TriangulationImpl)(const Point2d &pt1, const Point2d &pt2, const cv::Mat &camPrj1, const cv::Mat &camPrj2, cv::Mat &pt_4d);
    
    void PoseEstimation::estimateF_2d2d(const PtVector &pt1, const PtVector &pt2, cv::Mat &R, cv::Mat &t)
    {
        //根据基础矩阵计算本质矩阵
        Mat mask;
        Mat essential_matrix = findEssentialMat(pt1, pt2, mCamera.focal_length,mCamera.principal_point,RANSAC, 0.999, 1.0, mask);
        
        //通过本质矩阵推算相机R 和 t
        recoverPose( essential_matrix, pt1, pt2, R, t, mCamera.focal_length,mCamera.principal_point,mask);
    }
    
    void PoseEstimation::triangulation(const Point2f &pixel_1, const Point2f &pixel_2, float scale, const cv::Mat &R, const cv::Mat &t, Point3d &output)
    {
        Mat T1 = (Mat_<double>(3,4) << 1,0,0,0,
                  0,1,0,0,
                  0,0,1,0);
        Mat T2 = (Mat_<double>(3,4) <<
                  R.at<double>(0,0),R.at<double>(0,1),R.at<double>(0,2),t.at<double>(0,0) ,
                  R.at<double>(1,0),R.at<double>(1,1),R.at<double>(1,2),t.at<double>(1,0) ,
                  R.at<double>(2,0),R.at<double>(2,1),R.at<double>(2,2),t.at<double>(2,0) );
        
        Mat pt_4d;
        
        //像素坐标转相机物理坐标
        Point2d dp1 = Functions::Pixel2Cam(pixel_1, mCamera.K);
        Point2d dp2 = Functions::Pixel2Cam(pixel_2, mCamera.K);
        
        TriangulationImpl impl =  &PoseEstimation::triangulationCvImpl;
        
        //三角测量
        (this->*impl)(dp1,dp2,T1,T2,pt_4d);
        
        Mat x = pt_4d.col(0);
        
        x = x/x.at<double>(3,0);
        output = Point3d(x.at<double>(0,0),x.at<double>(1,0),x.at<double>(2,0));

        //尺度映射
        double d = scale / sqrt(t.at<double>(0,0) * t.at<double>(0,0)+
                                t.at<double>(1,0) * t.at<double>(1,0)+
                                t.at<double>(2,0) * t.at<double>(2,0));
        
        output = output * d;
    }
    
    void  PoseEstimation::triangulationCvImpl(const Point2d &pt1, const Point2d &pt2, const cv::Mat &camPrj1, const cv::Mat &camPrj2, cv::Mat &pt_4d)
    {
        std::vector<Point2d> pts_1, pts_2;
        
        pts_1.push_back(pt1);
        pts_2.push_back(pt2);
        
        triangulatePoints(camPrj1, camPrj2, pts_1, pts_2, pt_4d);
    }
    
    void PoseEstimation::triangulationLinearLSTImpl(const Point2d &pt1, const Point2d &pt2, const cv::Mat &camPrj1, const cv::Mat &camPrj2, cv::Mat &pt_4d)
    {
//        Mat tmp_pt = IterativeLinearLSTriangulation(Point3d(pt1.x,pt1.y,0), T1, Point3d(pt2.x,pt2.y,0), T2);
//
//        pt_4d = tmp_pt.clone();
    }
    
    
    
    /*
     * 计算基础矩阵
     */
    Mat PoseEstimation::findFundamentalMat(const PtVector &pt1,const PtVector &pt2,float &score)
    {
        //此处暂时直接调用cv接口  并未对score进行计算 待后续优化
        
        return cv::findFundamentalMat( pt1, pt2);
        
//        通过基础矩阵对结果进行筛选
//        std::vector<uchar> status;
//        Mat fm = cv::findFundamentalMat( pt1, pt2, CV_RANSAC,3,0.99,status);
//        assert(status.size() == pt1.size());
//        PtVector leftInlier;
//        PtVector rightInlier;
//
//        int index = 0;
//        for (unsigned i = 0; i < pt1.size(); i++) {
//            if (status[i] != 0){
//                leftInlier.push_back(pt1[i]);
//                rightInlier.push_back(pt2[i]);
//            }
//        }
//
//        pt1.swap(leftInlier);
//        pt2.swap(rightInlier);
        
//        return fm;
        
    }
    
    void PoseEstimation::calcEpiline(const Mat &fdMat,const Point2f &piexl, float &a, float &b, float &c)
    {        
        std::vector<cv::Vec<float, 3>>  epilines;
        
        PtVector pts{ piexl };
        //极线计算
        computeCorrespondEpilines(pts, 1, fdMat, epilines);
        
        a = epilines[0][0];
        b = epilines[0][1];
        c = epilines[0][2];
    }
    
     void PoseEstimation::calcEpiline(const PtVector &pt1, const PtVector &pt2,const PtVector &piexls, FloatVector &a, FloatVector &b, FloatVector &c)
    {
        Mat fdmatrix = cv::findFundamentalMat( pt1, pt2);//, CV_FM_8POINT);
        
        std::vector<cv::Vec<float, 3>>  epilines;
        
        //极线计算
        computeCorrespondEpilines(piexls, 1, fdmatrix, epilines);
        
        for( auto epiline :  epilines)
        {
            a.emplace_back(epiline[0]);
            b.emplace_back(epiline[1]);
            c.emplace_back(epiline[2]);
        }
    }
    //y = (-a*x - c) / b   计算y值
    inline double computeY(float x, float a, float b, float c)
    {
        assert( fabs(b) > 1e-6);
        return (-c - a * x) / b;
    }
    
    Point2f PoseEstimation::epilineSearch(const Mat &img1,const Mat &img2,const Point2f &piexl, float a,float b, float c,eBlockMatchingType type /*= eNCC*/)
    {
        Ptr<BlockMatch> pBlock = BlockMatch::CreateMethod(type, img1, piexl);
        
        const float len   = EPILINESEARCHLEN;
        const float space = EPILINESEARCHSP;
        float best_ncc    = -1.0;
        Point2f best_px_curr ;
        
        const float searchlen = piexl.x + len;
        for( float x = piexl.x ; x < searchlen; x += space)
        {
            Point2d px_curr(x, computeY(x, a,b,c));
            
            double ncc=  pBlock->score(img2, px_curr);
            
            if(ncc > best_ncc)
            {
                best_ncc = ncc;
                best_px_curr = px_curr;
            }
        }
        
        if( best_ncc < EPILINESEARCHSC)//判定阀值
        {//评分过低  认为没匹配到
            return Point2f(WRONGVALUE,WRONGVALUE);
        }
        else
        {
            return best_px_curr;
        }
    }
    
    
    TargetItem PoseEstimation::epilineSearch(const TargetItems &targets,const TargetItem &item,float a,float b, float c)
    {
        Point2f bg = item._center;
        bg.y = computeY(item._center.x, a, b, c);
        Point2f ed(item._center.x + EPILINESEARCHLEN,0);
        ed.y = computeY(ed.x, a, b, c);
        for( TargetItem target : targets)
        {
            if( target._id == item._id )
            {//检测到类型相同的 才进行下一步判断
                if(Functions::IsIntersect(bg, ed, target._box ))
                {//相交则认为是同一个物体,并不再进行下一步判断
                    return target;
                }
            }
        }
        return {0,Point2f(WRONGVALUE,WRONGVALUE),Rect2f()};
    }
    
    GeoPos PoseEstimation::calcWorldPos(const GeoPos &preGps, const GeoPos &curGps,const Point3d &target,Mat &tcw)
    {
        cv::Point3d pt1 = Functions::ComputeXYZFromGPS(preGps.x, preGps.y);
        cv::Point3d pt2 = Functions::ComputeXYZFromGPS(curGps.x, curGps.y);
        
        cv::Point3d dir = pt2 - pt1;
        
        Functions::Normalize(dir);
        
        cv::Point3d zAxis = pt2 - pt1;       //行驶方向(相机拍摄方向）
        Functions::Normalize(zAxis);         //单位化
        
        cv::Point3d yAxis =  pt1;            //up方向
        Functions::Normalize(yAxis);         //单位化
        
        cv::Point3d xAxis = zAxis.cross(yAxis);
        Functions::Normalize(xAxis);
        
        yAxis = zAxis.cross(xAxis);
        Functions::Normalize(yAxis);
        
#if  0
        yAxis = -yAxis;                       //y轴取反,因为相机y方向相反
        
        tcw = Functions::ComputeWorldTransMatrix(xAxis, yAxis,zAxis,pt1);//获取世界变换矩阵

        Mat pos = (Mat_<double>(4,1) << target.x,target.y,target.z,1) ;
        
        
        
        
        Mat rst = tcw * pos;
        
        cv::Point3d rstpt(rst.at<double>(0,0)/rst.at<double>(3,0),
                          rst.at<double>(1,0)/rst.at<double>(3,0),
                          rst.at<double>(2,0)/rst.at<double>(3,0));
#else
        Point3d rstpt;
        
        rstpt = pt1 + zAxis * target.z;
        rstpt = rstpt + xAxis * target.x;
        
        
#endif
    
       return Functions::ComputeGPSFromXYZ(rstpt);
    }
    
    void PoseEstimation::recover(Monocular::Frame *preFrame, Monocular::Frame *curFrame, const PtVector &prepts, const PtVector &curpts, cv::Mat &R, cv::Mat &t)
    {
        assert(preFrame);
        assert(curFrame);
        
        float score = 0.0;
        Mat fdMat = findFundamentalMat(prepts, curpts, score);
        
        //这里暂时直接用本质矩阵进行推导相机R t 后续看情况优化
        estimateF_2d2d(prepts, curpts, R, t);
    }
    
    Mat PoseEstimation::estimate(Frame *preFrame,Frame *curFrame,const PtVector &prepts,const PtVector &curpts,Serialization *pSer,float realscale/* = -1.0*/)
    {
        assert(preFrame);
        assert(curFrame);
        
        TimeInterval ti;
        float score = 0.0;
        
        Mat fdMat = findFundamentalMat(prepts,curpts,score);

        if(realscale < 0.0)
        {//若尺度未输入 或 有误 取两帧经纬度距离
            realscale = Functions::ComputeDistance(preFrame->getPosition(), curFrame->getPosition());
        }
        
        Mat R,t;
        //这里暂时直接用本质矩阵进行推导相机R t 后续看情况优化
        estimateF_2d2d(prepts, curpts, R, t);
        
#if NEEDWRITEFILE
        
        assert(pSer);
        pSer->prompt("-----------------------------------------------");
        pSer->writeFormat("preFrame Position", preFrame->getPosition());
        pSer->writeFormat("curFrame Position", curFrame->getPosition());
        pSer->writeFormat("R:", R);
        pSer->writeFormat("t:", t);
#endif
        
        TargetItems &preItems = preFrame->getTargetItems();
        TargetItems &curItems = curFrame->getTargetItems();
        
        for( TargetItem &target : preItems)
        {
            //极线a,b,c
            float a;
            float b;
            float c;

            if(!target.isValid())
            {//无效 则表示目标绝对坐标没有被恢复 或 计算的经纬度有误？

                //计算极线
                calcEpiline(fdMat, target._center, a, b, c);
                
#if TESTOUTPUT
                const_cast<Frame*>(preFrame)->drawLine(a, b, c);
                
                const_cast<Frame*>(preFrame)->drawMatch(prepts, curpts);
                
                Point2f bg = target._center;
                bg.y = computeY(target._center.x, a, b, c);
                Point2f ed(target._center.x + EPILINESEARCHLEN,0);
                ed.y = computeY(ed.x, a, b, c);
                
                const_cast<Frame*>(preFrame)->drawLine(bg,ed);
#endif
                
                TargetItem sm_target = epilineSearch(curItems, target, a, b, c);
                const Point2f &corrPt = sm_target._center;
                if(CHECKVALUE(corrPt))// > 0  认为是有效值
                {//找到同名点
                    Point3d output;
                    triangulation(target._center, corrPt, realscale, R, t, output);
                    if(output.z < 0 )output = -output;//add by tu. z<0 表示计算在车后 取反
                    Mat tcw;
                    GeoPos pos = calcWorldPos(preFrame->getPosition(), curFrame->getPosition(), output,tcw);
                    
                    target._pos = pos;
                    
#if NEEDWRITEFILE
                    pSer->writeFormat("calc pos", output);
                    
#if TESTOUTPUT
                    float dt = Functions::ComputeDistance(target._realpos, preFrame->getPosition()) - Functions::GetLength(output);

                    pSer->writeFormat("distance",dt);
#endif
                    pSer->prompt("target matching");
                    pSer->writeFormat("targetID", target._id);
                    pSer->writeFormat("first  center", target._center);
                    pSer->writeFormat("second center", corrPt);
                   
                    pSer->prompt("epiline");
                    pSer->writeFormat("a", a);
                    pSer->writeFormat("b", b);
                    pSer->writeFormat("c", c);
                    
                    pSer->writeFormat("relative length ", Functions::GetLength(output));;
                    
                    pSer->writeFormat("calc Position", target._pos);
                   
#if  TESTOUTPUT
                    target.a = a;
                    target.b = b;
                    target.c = c;
                    
                    const_cast<Frame*>(preFrame)->drawTargetItem( sm_target );//绘制匹配到对象的包围盒
                    
                    if(CHECKVALUE(target._realpos))//真实值有效
                    {
                        pSer->writeFormat("real Position", target._realpos);
                        pSer->writeFormat("distance(m)",Functions::ComputeDistance(target._pos, target._realpos));
                    }
#endif
                   
                    pSer->prompt("-----------------------------------------------");
                    
#endif
                    
                    ti.print("estimation");
                    
                    return tcw;
                }
                else
                {
#if NEEDWRITEFILE
                    pSer->writeFormat("Could not found target ",target._id);
#endif
                }
            }
            return Mat();
        }

        return Mat();
    }
    
    
    
    
    /***********************************block matching*****************************/
    
    /*
     * 创建对象
     */
    BlockMatch* BlockMatch::CreateMethod(eBlockMatchingType type,const Mat &img, const Point2f &pt)
    {
        switch (type) {
            case eNCC:
                return new NCC_BlockMatch(img,pt);
            //add more
            default:
                return NULL;
        }
    }
    
    NCC_BlockMatch::NCC_BlockMatch(const Mat &mat,const Point2f &pt):_mat(mat)
    {
        // 零均值-归一化互相关
        // 先算均值
        for ( int x=-ncc_window_size; x<=ncc_window_size; x++ )
            for ( int y=-ncc_window_size; y<=ncc_window_size; y++ )
            {
                double value_ref = double(mat.ptr<uchar>( int(y+pt.y) )[ int(x+pt.x) ])/255.0;
                _mean += value_ref;
                
                _values.push_back(value_ref);
                
            }
        
        _mean /= ncc_area;
        
        // 计算 Zero mean NCC
        for ( int i=0; i<_values.size(); i++ )
        {
            _demoniator += (_values[i]-_mean)*(_values[i]-_mean);
        }
    }
    
    double NCC_BlockMatch::score(const Mat &cur,const Point2f &pt)
    {
        // 零均值-归一化互相关
        // 先算均值
        double mean_ref = 0, mean_curr = 0;
        std::vector<double> values_curr; // 参考帧和当前帧的均值
        for ( int x=-ncc_window_size; x<=ncc_window_size; x++ )
            for ( int y=-ncc_window_size; y<=ncc_window_size; y++ )
            {
                double value_curr = getBilinearInterpolatedValue( cur, pt+Point2f(x,y) );
                mean_curr += value_curr;
                
                values_curr.push_back(value_curr);
            }
        
        mean_curr /= ncc_area;
        
        // 计算 Zero mean NCC
        double numerator = 0, cur_demoniator = 0;
        for ( int i=0; i < _values.size(); i++ )
        {
            double n = (_values[i]-mean_ref) * (values_curr[i]-mean_curr);
            numerator += n;
            cur_demoniator += (values_curr[i]-mean_curr)*(values_curr[i]-mean_curr);
        }
        return numerator / sqrt( _demoniator * cur_demoniator+1e-10 );   // 防止分母出现零
    }
}
