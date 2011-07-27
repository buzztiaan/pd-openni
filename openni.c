/*
first attempt at an OpenNI pd interface

- buZz [Puik.]
  http://puikheid.nl/
  started; 26-07-2011

  mashup of pd's LibraryTemplate and OpenNi's NiUserTracker

*/


#include <pdextended/m_pd.h>
#include <pdextended/m_imp.h>

#include <ni/XnOpenNI.h>
#include <ni/XnCodecIDs.h>
#include <ni/XnCppWrapper.h>

#include <string.h>

xn::Context g_Context;
xn::ScriptNode g_scriptNode;
xn::DepthGenerator g_DepthGenerator;
xn::UserGenerator g_UserGenerator;
xn::Player g_Player;

XnBool g_bNeedPose = FALSE;
XnChar g_strPose[20] = "";

#define CONFIG_XML_PATH "/pd-openni.xml"

#define CHECK_RC(nRetVal, what)									\
	if (nRetVal != XN_STATUS_OK)								\
	{											\
		post("openni: %s failed: %s", what, xnGetStatusString(nRetVal));		\
	}
/*		return nRetVal;									\
	}
*/

   /* the data structure for each copy of "openni".  In this case we
   only need pd's obligatory header (of type t_object). */
typedef struct openni
{
  t_object x_ob;
} t_openni;


    /* this is a pointer to the class for "openni", which is created in the
    "setup" routine below and used to create new ones in the "new" routine. */
t_class *openni_class;



void XN_CALLBACK_TYPE MyCalibrationInProgress(xn::SkeletonCapability& capability, XnUserID id, XnCalibrationStatus calibrationError, void* pCookie)
{
//	# m_Errors[id].first = calibrationError;
}
void XN_CALLBACK_TYPE MyPoseInProgress(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID id, XnPoseDetectionStatus poseError, void* pCookie)
{
//	# m_Errors[id].second = poseError;
}

// Callback: New user was detected
void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	post("openni: New User %d", nId);
	// New user found
	if (g_bNeedPose)
	{
		g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
	}
	else
	{
		g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
	}
}


// Callback: An existing user was lost
void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	post("openni: Lost user %d", nId);
}

// Callback: Detected a pose
void XN_CALLBACK_TYPE UserPose_PoseDetected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	post("openni: Pose %s detected for user %d", strPose, nId);
	g_UserGenerator.GetPoseDetectionCap().StopPoseDetection(nId);
	g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
}

// Callback: Started calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationStart(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie)
{
	post("openni: Calibration started for user %d", nId);
}

// Callback: Finished calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationEnd(xn::SkeletonCapability& capability, XnUserID nId, XnBool bSuccess, void* pCookie)
{
	if (bSuccess)
	{
		// Calibration succeeded
		post("openni: Calibration complete, start tracking user %d", nId);
		g_UserGenerator.GetSkeletonCap().StartTracking(nId);
	}
	else
	{
		// Calibration failed
		post("openni: Calibration failed for user %d", nId);
		if (g_bNeedPose)
		{
			g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
		}
		else
		{
			g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}

void XN_CALLBACK_TYPE UserCalibration_CalibrationComplete(xn::SkeletonCapability& capability, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie)
{
	if (eStatus == XN_CALIBRATION_STATUS_OK)
	{
		// Calibration succeeded
		post("openni: Calibration complete, start tracking user %d", nId);
		g_UserGenerator.GetSkeletonCap().StartTracking(nId);
	}
	else
	{
		// Calibration failed
		post("openni: Calibration failed for user %d", nId);
		if (g_bNeedPose)
		{
			g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
		}
		else
		{
			g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}


void update_openni(void)
{
	XnStatus nRetVal = XN_STATUS_OK;

	nRetVal = g_Context.WaitOneUpdateAll(g_DepthGenerator);


	if (nRetVal != XN_STATUS_OK)
	{
		post("openni: Failed updating data");
	}

//	g_DepthGenerator.GetMetaData(depthMD);
//	g_UserGenerator.GetUserPixels(0, sceneMD);
}

    /* this is called when openni gets the message, "rats". */
extern "C" void openni_bang(t_openni *x)
{
    update_openni();
    x=NULL; /* don't warn about unused variables */
}

    /* this is called back when openni gets a "float" message (i.e., a
    number.) */
extern "C" void openni_float(t_openni *x, t_floatarg f)
{
    post("openni: %f", f);
    x=NULL; /* don't warn about unused variables */
}


void start_openni(void)
{

	XnStatus nRetVal = XN_STATUS_OK;

	xn::EnumerationErrors errors;

	nRetVal = g_Context.InitFromXmlFile(strncat(( openni_class -> c_externdir -> s_name ),CONFIG_XML_PATH,20), g_scriptNode, &errors);
	if (nRetVal == XN_STATUS_NO_NODE_PRESENT)
	{
		XnChar strError[1024];
		errors.ToString(strError, 1024);
		post("openni: %s", strError);
//			return (nRetVal);
	}
	else if (nRetVal != XN_STATUS_OK)
	{
		post("openni: Open failed: %s", xnGetStatusString(nRetVal));
//		return (nRetVal);
	}

	nRetVal = g_Context.FindExistingNode(XN_NODE_TYPE_DEPTH, g_DepthGenerator);
	CHECK_RC(nRetVal, "Find depth generator");
	nRetVal = g_Context.FindExistingNode(XN_NODE_TYPE_USER, g_UserGenerator);
	if (nRetVal != XN_STATUS_OK)
	{
		nRetVal = g_UserGenerator.Create(g_Context);
		CHECK_RC(nRetVal, "Find user generator");
	}

	XnCallbackHandle hUserCallbacks, hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress;
	if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON))
	{
		post("openni: Supplied user generator doesn't support skeleton");
//		return 1;
	}
	nRetVal = g_UserGenerator.RegisterUserCallbacks(User_NewUser, User_LostUser, NULL, hUserCallbacks);
	CHECK_RC(nRetVal, "Register to user callbacks");
	nRetVal = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationStart(UserCalibration_CalibrationStart, NULL, hCalibrationStart);
	CHECK_RC(nRetVal, "Register to calibration start");
	nRetVal = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationComplete(UserCalibration_CalibrationComplete, NULL, hCalibrationComplete);
	CHECK_RC(nRetVal, "Register to calibration complete");

	if (g_UserGenerator.GetSkeletonCap().NeedPoseForCalibration())
	{
		g_bNeedPose = TRUE;
		if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
		{
			post("openni: Pose required, but not supported");
//			return 1;
		}
		nRetVal = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseDetected(UserPose_PoseDetected, NULL, hPoseDetected);
		CHECK_RC(nRetVal, "Register to Pose Detected");
		g_UserGenerator.GetSkeletonCap().GetCalibrationPose(g_strPose);
	}

	g_UserGenerator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

	nRetVal = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationInProgress(MyCalibrationInProgress, NULL, hCalibrationInProgress);
	CHECK_RC(nRetVal, "Register to calibration in progress");

	nRetVal = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseInProgress(MyPoseInProgress, NULL, hPoseInProgress);
	CHECK_RC(nRetVal, "Register to pose in progress");

	nRetVal = g_Context.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");

}

    /* this is called when a new "openni" object is created. */
extern "C" void *openni_new(void)
{
    t_openni *x = (t_openni *)pd_new(openni_class);
    post("[openni] initializing");
    start_openni();
    post("[openni] initialized");
    return (void *)x;
}


extern "C" void openni_setup(void)
{
    post("pd-openni [openni] object");
    post("2k11 buZz [Puik.] http://puikheid.nl/");

    openni_class = class_new(gensym("openni"), (t_newmethod)openni_new, 0, sizeof(t_openni), CLASS_PATCHABLE, A_DEFFLOAT,  0);
    class_addmethod(openni_class, (t_method)openni_bang, gensym("bang"), A_NULL);
    class_addfloat(openni_class, openni_float);
}

