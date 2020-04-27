/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "IntersectionTraversal.h"

#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/vk/Draw.h>
#include <vsg/vk/BindVertexBuffers.h>
#include <vsg/vk/BindIndexBuffer.h>
#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/maths/transform.h>

#include <iostream>

#include "LineSegmentIntersector.h"

using namespace vsg;

struct PushPopNode
{
    IntersectionTraversal::NodePath& nodePath;

    PushPopNode(IntersectionTraversal::NodePath& np, const Node* node) : nodePath(np) { nodePath.push_back(node); }
    ~PushPopNode() { nodePath.pop_back(); }
};

IntersectionTraversal::IntersectionTraversal()
{
    _topologyStack.push_back(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

void IntersectionTraversal::apply(const Node& node)
{
    PushPopNode ppn(_nodePath, &node);

    node.traverse(*this);
}

void IntersectionTraversal::apply(const StateGroup& stategroup)
{
    PushPopNode ppn(_nodePath, &stategroup);

    auto previous_topology = topology();

    struct FindGraphicsPipelineVisitor : public ConstVisitor
    {
        VkPrimitiveTopology topology;

        FindGraphicsPipelineVisitor(VkPrimitiveTopology in_topology) : topology(in_topology) {}

        void apply(const BindGraphicsPipeline& bpg) override
        {
            for(auto& pipelineState : bpg.getPipeline()->getPipelineStates())
            {
                if (auto ias = pipelineState.cast<InputAssemblyState>(); ias) topology = ias->topology;
            }
        }
    } findGraphicsPipeline(previous_topology);

    for(auto& state : stategroup.getStateCommands())
    {
        state->accept(findGraphicsPipeline);
    }

    if (findGraphicsPipeline.topology != previous_topology)
    {
        _topologyStack.push_back(findGraphicsPipeline.topology);
    }

    stategroup.traverse(*this);

    if (findGraphicsPipeline.topology != previous_topology)
    {
        _topologyStack.pop_back();
    }
}

void IntersectionTraversal::apply(const MatrixTransform& transform)
{
    PushPopNode ppn(_nodePath, &transform);

    pushTransform(transform.getMatrix());

    transform.traverse(*this);

    popTransform();
}



void IntersectionTraversal::apply(const LOD& lod)
{
    PushPopNode ppn(_nodePath, &lod);

    if (intersects(lod.getBound()))
    {
        for(auto& child : lod.getChildren())
        {
            if (child.node)
            {
                child.node->accept(*this);
                break;
            }
        }
    }
}

void IntersectionTraversal::apply(const PagedLOD& plod)
{
    PushPopNode ppn(_nodePath, &plod);

    if (intersects(plod.getBound()))
    {
        for(auto& child : plod.getChildren())
        {
            if (child.node)
            {
                child.node->accept(*this);
                break;
            }
        }
    }
}

void IntersectionTraversal::apply(const CullNode& cn)
{
    PushPopNode ppn(_nodePath, &cn);

    if (intersects(cn.getBound())) cn.traverse(*this);
}

void IntersectionTraversal::apply(const VertexIndexDraw& vid)
{
    if (vid.arrays.empty()) return;

    PushPopNode ppn(_nodePath, &vid);

    sphere bound;
    if (!vid.getValue("bound", bound))
    {
        box bb;
        if (auto vertices = vid.arrays[0].cast<vec3Array>(); vertices)
        {
            for(auto& vertex : *vertices) bb.add(vertex);
        }

        if (bb.valid())
        {
            bound.center = (bb.min + bb.max) * 0.5f;
            bound.radius = length(bb.max - bb.min) * 0.5f;

            // hacky but better to reuse results.  Perhaps use a std::map<> to avoid a breaking const, or make the vistitor non const?
            const_cast<VertexIndexDraw&>(vid).setValue("bound", bound);
        }

        // std::cout<<"Computed bounding sphere : "<<bound.center<<", "<<bound.radius<<std::endl;
    }
    else
    {
        // std::cout<<"Found bounding sphere : "<<bound.center<<", "<<bound.radius<<std::endl;
    }

    if (intersects(bound))
    {
        intersect(topology(), vid.arrays, vid.indices, vid.firstIndex, vid.indexCount);
    }
}

void IntersectionTraversal::apply(const Geometry& geometry)
{
    PushPopNode ppn(_nodePath, &geometry);

    _arrays = geometry.arrays;
    _indices = geometry.indices;

    for(auto& command : geometry.commands)
    {
        command->accept(*this);
    }
}


void IntersectionTraversal::apply(const BindVertexBuffers& bvb)
{
    _arrays = bvb.getArrays();
}

void IntersectionTraversal::apply(const BindIndexBuffer& bib)
{
    _indices = bib.getIndices();
}

void IntersectionTraversal::apply(const Draw& draw)
{
    PushPopNode ppn(_nodePath, &draw);

    intersect(topology(), _arrays, draw.firstVertex, draw.vertexCount);
}

void IntersectionTraversal::apply(const DrawIndexed& drawIndexed)
{
    PushPopNode ppn(_nodePath, &drawIndexed);

    intersect(topology(), _arrays, _indices, drawIndexed.firstIndex, drawIndexed.indexCount);
}
