#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/maths/transform.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/PointerEvent.h>
#include <vsg/ui/ScrollWheelEvent.h>
#include <vsg/viewer/Camera.h>
#include <vsg/viewer/EllipsoidModel.h>

namespace vsg
{
    class VSG_DECLSPEC GlobeTrackball : public Inherit<Visitor, GlobeTrackball>
    {
    public:
        GlobeTrackball(ref_ptr<Camera> camera, ref_ptr<EllipsoidModel> ellipsoidModel = {});

        /// compute non dimensional window coordinate (-1,1) from event coords
        dvec2 ndc(PointerEvent& event);

        /// compute trackball coordinate from event coords
        dvec3 tbc(PointerEvent& event);

        void apply(KeyPressEvent& keyPress) override;
        void apply(ButtonPressEvent& buttonPress) override;
        void apply(ButtonReleaseEvent& buttonRelease) override;
        void apply(MoveEvent& moveEvent) override;
        void apply(ScrollWheelEvent& scrollWheel) override;
        void apply(FrameEvent& frame) override;

        void rotate(double angle, const dvec3& axis);
        void zoom(double ratio);
        void pan(const dvec2& delta);

        bool withinRenderArea(int32_t x, int32_t y) const;

        void clampToGlobe();

        // TODO user controls for mouse button bindings and scales and button hold motion

    protected:
        ref_ptr<Camera> _camera;
        ref_ptr<LookAt> _lookAt;
        ref_ptr<LookAt> _homeLookAt;
        ref_ptr<EllipsoidModel> _ellipsoidModel;

        bool _hasFocus = false;
        bool _lastPointerEventWithinRenderArea = false;

        enum UpdateMode
        {
            INACTIVE = 0,
            ZOOM,
            PAN,
            ROTATE
        };
        UpdateMode _updateMode = INACTIVE;
        double _zoomPreviousRatio = 0.0;
        dvec2 _pan;
        double _rotateAngle = 0.0;
        dvec3 _rotateAxis;

        KeySymbol _homeKey = KEY_Space;
        double _direction;

        bool first_frame = true;
        dvec2 prev_ndc;
        dvec3 prev_tbc;
        time_point prev_time;
    };

} // namespace vsg
