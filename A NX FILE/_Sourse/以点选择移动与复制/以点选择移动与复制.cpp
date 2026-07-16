//==============================================================================
//       Filename:  以点选择移动与复制.cpp
//        Created by: Lttt   Version: NX 2306   Date: 07-15-2026
//==============================================================================

#pragma warning(disable : 4275)
#include "以点选择移动与复制.hpp"
#include <cmath>
#include <cstring>
#include <uf.h>
#include <uf_modl.h>
#include <uf_trns.h>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <uf_disp.h>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/WCS.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/MeasureBodies.hxx>
#include <NXOpen/MeasureBodyBuilder.hxx>
#include <NXOpen/SelectDisplayableObjectList.hxx>

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

Session* 以点选择移动与复制::theSession = NULL;
UI* 以点选择移动与复制::theUI = NULL;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
以点选择移动与复制::以点选择移动与复制()
{
    try
    {
        theSession = NXOpen::Session::GetSession();
        theUI = UI::GetUI();
        theDlxFileName = "以点选择移动与复制.dlx";
        theDialog = theUI->CreateDialog(theDlxFileName);
        // Buttons "移动"/"复制" set mode via update_cb; then Apply/OK executes
        theDialog->AddApplyHandler(make_callback(this, &以点选择移动与复制::apply_cb));
        theDialog->AddOkHandler(make_callback(this, &以点选择移动与复制::ok_cb));
        theDialog->AddUpdateHandler(make_callback(this, &以点选择移动与复制::update_cb));
        theDialog->AddInitializeHandler(make_callback(this, &以点选择移动与复制::initialize_cb));
        theDialog->AddDialogShownHandler(make_callback(this, &以点选择移动与复制::dialogShown_cb));
        m_isCopy = false;
        m_fromModeButton = false;
    }
    catch(exception&)
    {
        throw;
    }
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
以点选择移动与复制::~以点选择移动与复制()
{
    if (theDialog != NULL)
    {
        delete theDialog;
        theDialog = NULL;
    }
}

//------------------------------------------------------------------------------
// ufusr entry point
//------------------------------------------------------------------------------
extern "C" DllExport void ufusr(char *param, int *retcod, int param_len)
{
    以点选择移动与复制 *theApp = NULL;
    try
    {
        theApp = new 以点选择移动与复制();
        theApp->Launch();
    }
    catch(exception& ex)
    {
        以点选择移动与复制::theUI->NXMessageBox()->Show("Error",
            NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    if(theApp != NULL)
    {
        delete theApp;
        theApp = NULL;
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return (int)Session::LibraryUnloadOptionImmediately;
}

extern "C" DllExport void ufusr_cleanup(void) { }

NXOpen::BlockStyler::BlockDialog::DialogResponse 以点选择移动与复制::Launch()
{
    return theDialog->Launch();
}

//==============================================================================
// Helper: get length unit (millimeter)
//==============================================================================
static NXOpen::Point3d GetBodyCentroid(NXOpen::Body* body)
{
    Point3d pt(0,0,0);
    if (!body) return pt;

    tag_t bodyTag = body->Tag();
    if (bodyTag == NULL_TAG) return pt;

    UF_initialize();

    tag_t objects[1] = {bodyTag};
    int  num_objs = 1;
    int  type = 1;              // solid body
    int  units = 4;             // kg-m
    double density = 1.0;
    int  acc_method = 1;        // use accuracy value
    double acc_value[11] = {0.001, 0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
    double mass_props[47] = {0.0};
    double stats[16] = {0.0};

    int err = UF_MODL_ask_mass_props_3d(
        objects, num_objs, type, units, density,
        acc_method, acc_value, mass_props, stats);

    if (err == 0)
    {
        pt.X = mass_props[0];
        pt.Y = mass_props[1];
        pt.Z = mass_props[2];
    }
    return pt;
}

//==============================================================================
//==============================================================================
static void GetBodyCenterZRange(NXOpen::Body* body, NXOpen::Point3d& centroid, double& zMin, double& zMax)
{
    centroid = GetBodyCentroid(body);
    zMin = centroid.Z;
    zMax = centroid.Z;
}

//==============================================================================
//  Helper: compute face center point via MeasureBodyBuilder (pure NXOpen)
//==============================================================================
static NXOpen::Point3d GetFaceCenter(NXOpen::Face* face)
{
    if (!face) return Point3d(0,0,0);
    Point3d pt(0,0,0);
    try
    {
        Session* sess = Session::GetSession();
        Part* workPart = sess->Parts()->Work();
        MeasureBodyBuilder* builder = workPart->MeasureManager()->CreateMeasureBodyBuilder(nullptr);
        if (builder)
        {
            auto* objList = builder->BodyObjects();
            if (objList) { objList->Add(reinterpret_cast<NXOpen::DisplayableObject*>(face)); }
            NXOpen::NXObject* result = builder->Commit();
            NXOpen::MeasureBodies* mb = dynamic_cast<NXOpen::MeasureBodies*>(result);
            if (mb) { pt = mb->Centroid(); }
            builder->Destroy();
        }
    }
    catch (...) {}
    return pt;
}


//==============================================================================
// Helper: check if selected objects are all faces (vs bodies)
//==============================================================================
//==============================================================================
// Helper: get first Body from selection
//==============================================================================
static NXOpen::Body* GetFirstBody(NXOpen::BlockStyler::SelectObject* sel)
{
    auto objs = sel->GetSelectedObjects();
    for (auto obj : objs)
    {
        // Try direct Body cast
        NXOpen::Body* b = dynamic_cast<NXOpen::Body*>(obj);
        if (b) return b;
        // If it's a Feature (Extrude, Revolve, etc.), get the body from it
        NXOpen::Features::Feature* f = dynamic_cast<NXOpen::Features::Feature*>(obj);
        if (f) {
            auto bodies = f->GetBodies();
            if (!bodies.empty()) return bodies[0];
        }
    }
    return NULL;
}

//==============================================================================
// Helper: get first Face from selection
//==============================================================================
static NXOpen::Face* GetFirstFace(NXOpen::BlockStyler::SelectObject* sel)
{
    auto objs = sel->GetSelectedObjects();
    for (auto obj : objs)
    {
        NXOpen::Face* f = dynamic_cast<NXOpen::Face*>(obj);
        if (f) return f;
    }
    return NULL;
}

//==============================================================================
// Helper: compute start point from enum0 value + selected objects
//==============================================================================
static NXOpen::Point3d ComputeStartPoint(
    int radioVal, NXOpen::BlockStyler::SelectObject* sel)
{
    Point3d pt(0,0,0);
    try
    {
        switch (radioVal)
        {
            case 0: // 体中心上
            {
                NXOpen::Body* body = GetFirstBody(sel);
                if (body)
                {
                    Point3d centroid(0,0,0); double zMin=0, zMax=0;
                    GetBodyCenterZRange(body, centroid, zMin, zMax);
                    pt = Point3d(centroid.X, centroid.Y, zMax);
                }
                break;
            }
            case 1: // 体中心
            {
                NXOpen::Body* body = GetFirstBody(sel);
                if (body) pt = GetBodyCentroid(body);
                break;
            }
            case 2: // 体中心下
            {
                NXOpen::Body* body = GetFirstBody(sel);
                if (body)
                {
                    Point3d centroid(0,0,0); double zMin=0, zMax=0;
                    GetBodyCenterZRange(body, centroid, zMin, zMax);
                    pt = Point3d(centroid.X, centroid.Y, zMin);
                }
                break;
            }
            case 3: // 面中心
            {
                NXOpen::Face* face = GetFirstFace(sel);
                if (face) pt = GetFaceCenter(face);
                break;
            }
        }
    }
    catch (...) {}
    return pt;
}

//==============================================================================
// Helper: compute end point from enum01 value + selected objects
//==============================================================================
static NXOpen::Point3d ComputeEndPoint(
    int radioVal, NXOpen::BlockStyler::SelectObject* sel)
{
    Point3d pt(0,0,0);
    try
    {
        Session* sess = Session::GetSession();
        Part* workPart = sess->Parts()->Work();
        WCS* wcs = workPart->WCS();

        switch (radioVal)
        {
            case 0: // ABS原点
                pt = Point3d(0,0,0);
                break;
            case 1: // WCS原点
                pt = wcs->Origin();
                break;
            case 2: // 体中心
            {
                NXOpen::Body* body = GetFirstBody(sel);
                if (body) pt = GetBodyCentroid(body);
                break;
            }
            case 3: // 面中心
            {
                NXOpen::Face* face = GetFirstFace(sel);
                if (face) pt = GetFaceCenter(face);
                break;
            }
            case 4: // XO点
            {
                Point3d wcsOrigin = wcs->Origin();
                pt = Point3d(wcsOrigin.X + 5000.0, wcsOrigin.Y, wcsOrigin.Z);
                break;
            }
            case 5: // YO点
            {
                Point3d wcsOrigin = wcs->Origin();
                pt = Point3d(wcsOrigin.X, wcsOrigin.Y + 5000.0, wcsOrigin.Z);
                break;
            }
            case 6: // ZO点
            {
                Point3d wcsOrigin = wcs->Origin();
                pt = Point3d(wcsOrigin.X, wcsOrigin.Y, wcsOrigin.Z + 5000.0);
                break;
            }
        }
    }
    catch (...) {}
    return pt;
}

//==============================================================================
// Helper: read radio group value from an Enumeration (RadioBox) block
//==============================================================================
static int GetRadioValue(NXOpen::BlockStyler::Enumeration* radio)
{
    int val = 0;
    if (!radio) return 0;
    // Try 1: read Value as integer
    try {
        PropertyList* props = radio->GetProperties();
        if (props) {
            val = props->GetInteger("Value");
            delete props;
            return val;
        }
    } catch(...) {}
    // Try 2: read Value as string, find its index in layout
    try {
        NXString cur = radio->ValueAsString();
        std::vector<NXString> mem = radio->GetLayoutMembers();
        for (size_t i = 0; i < mem.size(); i++) {
            if (strcmp(cur.GetText(), mem[i].GetText()) == 0) { val = (int)i; return val; }
        }
    } catch(...) {}
    return val;
}

//==============================================================================
// Helper: update SpecifyPoint with computed point + handle interactive override
//==============================================================================
static void UpdateSpecifyPoint(
    NXOpen::BlockStyler::SpecifyPoint* ptBlock,
    const NXOpen::Point3d& computedPt,
    bool userPicked = false)
{
    if (!ptBlock) return;
    try
    {
        // If user interactively picked a point, keep it; else set computed
        if (!userPicked)
            ptBlock->SetPoint(computedPt);
    }
    catch(...) { }
}

//==============================================================================
// Helper: update sensitivity of radio items based on selection type
//   enum0: [体中心上, 体中心, 体中心下, 面中心]
//   enum01:   [ABS原点, WCS原点, 体中心, 面中心, XO点, YO点, ZO点]
//==============================================================================
//==============================================================================
// Callback: initialize_cb
//==============================================================================
void 以点选择移动与复制::initialize_cb()
{
    try
    {
        group0     = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group0"));
        moveButton  = dynamic_cast<Button*>(theDialog->TopBlock()->FindBlock("moveButton"));
        copyButton0 = dynamic_cast<Button*>(theDialog->TopBlock()->FindBlock("copyButton0"));
        group       = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group"));
        selectObject= dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("selectObject"));
        group1      = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group1"));
        point0      = dynamic_cast<SpecifyPoint*>(theDialog->TopBlock()->FindBlock("point0"));
        enum0       = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("enum0"));
        group2      = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group2"));
        point01     = dynamic_cast<SpecifyPoint*>(theDialog->TopBlock()->FindBlock("point01"));
        enum01      = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("enum01"));

        // Enable all options: get current sensitivity, then set all to 1
        if (enum0) {
            try {
                std::vector<int> sens = enum0->GetEnumSensitivity();
                for (size_t i = 0; i < sens.size(); i++) sens[i] = 1;
                enum0->SetEnumSensitivity(sens);
            } catch(...) {}
        }
        if (enum01) {
            try {
                std::vector<int> sens = enum01->GetEnumSensitivity();
                for (size_t i = 0; i < sens.size(); i++) sens[i] = 1;
                enum01->SetEnumSensitivity(sens);
            } catch(...) {}
        }
    }
    catch(exception& ex)
    {
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
}

//==============================================================================
// Callback: dialogShown_cb
//==============================================================================
void 以点选择移动与复制::dialogShown_cb()
{
    try
    {
        // Enable all options: get current sensitivity, then set all to 1
        if (enum0) {
            try {
                std::vector<int> sens = enum0->GetEnumSensitivity();
                for (size_t i = 0; i < sens.size(); i++) sens[i] = 1;
                enum0->SetEnumSensitivity(sens);
            } catch(...) {}
        }
        if (enum01) {
            try {
                std::vector<int> sens = enum01->GetEnumSensitivity();
                for (size_t i = 0; i < sens.size(); i++) sens[i] = 1;
                enum01->SetEnumSensitivity(sens);
            } catch(...) {}
        }

        // If nothing selected yet, pre-compute default points
        if (selectObject->GetSelectedObjects().empty())
        {
            point0->SetPoint(ComputeStartPoint(1, selectObject));  // 体中心 (default)
            point01->SetPoint(ComputeEndPoint(0, selectObject));      // ABS原点 (default)
        }
    }
    catch(exception& ex)
    {
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
}

//==============================================================================
// Perform the move/copy transform using MoveObjectBuilder (pure NXOpen API)
//==============================================================================
static int PerformTransform(
    NXOpen::BlockStyler::SelectObject* sel,
    const NXOpen::Point3d& srcPt,
    const NXOpen::Point3d& dstPt,
    bool isCopy)
{
    auto objects = sel->GetSelectedObjects();
    if (objects.empty()) return -1;

    double dx = dstPt.X - srcPt.X;
    double dy = dstPt.Y - srcPt.Y;
    double dz = dstPt.Z - srcPt.Z;

    // If delta is nearly zero, use small default offset (200mm in X)
    if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6 && std::abs(dz) < 1e-6)
    {
        dx = 200.0;
        dy = 0.0;
        dz = 0.0;
    }

    // Use MoveObjectBuilder (creates visible feature in part navigator)
    try
    {
        Session* session = Session::GetSession();
        Part* workPart = session->Parts()->Work();

        Features::MoveObjectBuilder* moveBuilder =
            workPart->BaseFeatures()->CreateMoveObjectBuilder(nullptr);
        if (!moveBuilder) return -1;

        SelectNXObjectList* selList = moveBuilder->ObjectToMoveObject();
        for (auto obj : objects)
        {
            NXOpen::NXObject* nxObj = dynamic_cast<NXOpen::NXObject*>(obj);
            if (nxObj) selList->Add(nxObj);
        }

        if (isCopy)
            moveBuilder->SetMoveObjectResult(
                Features::MoveObjectBuilder::MoveObjectResultOptionsCopyOriginal);
        else
            moveBuilder->SetMoveObjectResult(
                Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);

        GeometricUtilities::ModlMotion* motion = moveBuilder->TransformMotion();
        motion->SetOption(GeometricUtilities::ModlMotion::OptionsDeltaXyz);
        motion->DeltaXc()->SetNumberValue(dx);
        motion->DeltaYc()->SetNumberValue(dy);
        motion->DeltaZc()->SetNumberValue(dz);

        moveBuilder->Commit();
        moveBuilder->Destroy();

        Session::UndoMarkId markId = session->SetUndoMark(
            Session::MarkVisibilityInvisible, "Transform");
        session->UpdateManager()->DoUpdate(markId);

        return 0;
    }
    catch (NXOpen::NXException&)
    {
        return -1;
    }
}

//==============================================================================
// Callback: update_cb (called when any block value changes)
//==============================================================================
int 以点选择移动与复制::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    try
    {
        // Guard: skip if critical blocks are NULL
        if (!selectObject || !enum0 || !enum01 || !point0 || !point01)
            return 0;

        // --- Selection changed: update radio sensitivity ---
        if (block == selectObject)
        {
            // Auto-recompute start/end points based on current radio values
            int startVal = GetRadioValue(enum0);
            int endVal   = GetRadioValue(enum01);

            Point3d newStart = ComputeStartPoint(startVal, selectObject);
            Point3d newEnd   = ComputeEndPoint(endVal, selectObject);

            point0->SetPoint(newStart);
            point01->SetPoint(newEnd);
        }

        // --- Start point radio changed: recompute start point ---
        else if (block == enum0)
        {
            int val = GetRadioValue(enum0);
            Point3d pt = ComputeStartPoint(val, selectObject);
            point0->SetPoint(pt);
        }

        // --- End point radio changed: recompute end point ---
        else if (block == enum01)
        {
            int val = GetRadioValue(enum01);
            Point3d pt = ComputeEndPoint(val, selectObject);
            point01->SetPoint(pt);
        }

        // --- Start point manually picked by user: just accept ---
        else if (block == point0)
        {
            // User interactively picked a start point — keep it
        }

        // --- End point manually picked by user: just accept ---
        else if (block == point01)
        {
            // User interactively picked an end point — keep it
        }
        else if (block == moveButton)
        {
            m_isCopy = false;
            m_fromModeButton = true;
        }
        else if (block == copyButton0)
        {
            m_isCopy = true;
            m_fromModeButton = true;
        }

    }
    catch(exception& ex)
    {
        theUI->NXMessageBox()->Show("Block Styler",
            NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    return 0;
}

//==============================================================================
// Callback: apply_cb (execute move/copy on Apply or OK)
//==============================================================================
int 以点选择移动与复制::apply_cb()
{
    int errorCode = 0;
    try
    {
        auto objects = selectObject->GetSelectedObjects();
        if (objects.empty())
        {
            theUI->NXMessageBox()->Show("", NXOpen::NXMessageBox::DialogTypeError,
                "请先选择要操作的几何体。");
            return 1;
        }

        // Read start point from point0 (user can manually adjust)
        Point3d srcPt = point0->Point();
        Point3d dstPt = point01->Point();

        // Read mode: set by moveButton/copyButton0
        bool isCopy = m_isCopy;

        int ret = PerformTransform(selectObject, srcPt, dstPt, isCopy);
        if (ret != 0)
        {
            theUI->NXMessageBox()->Show("", NXOpen::NXMessageBox::DialogTypeError,
                "变换操作失败。");
        }
    }
    catch(exception& ex)
    {
        errorCode = 1;
        theUI->NXMessageBox()->Show("Block Styler",
            NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    return errorCode;
}

//==============================================================================
// Callback: ok_cb
//==============================================================================
int 以点选择移动与复制::ok_cb()
{
    return apply_cb();
}

//==============================================================================
// GetBlockProperties
//==============================================================================
PropertyList* 以点选择移动与复制::GetBlockProperties(const char *blockID)
{
    return theDialog->GetBlockProperties(blockID);
}
