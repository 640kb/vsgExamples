#include "TextGroup.h"

using namespace vsg;

int TextGroup::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer_container(children, rhs.children);
}

void TextGroup::read(Input& input)
{
    Node::read(input);

    input.readObjects("children", children);
}

void TextGroup::write(Output& output) const
{
    Node::write(output);

    output.writeObjects("children", children);
}

void TextGroup::setup(uint32_t minimumAllocation)
{
    if (new_implementation)
    {
        struct CountQuads : public ConstVisitor
        {
            size_t count = 0;

            void apply(const stringValue& text) override
            {
                count += text.value().size();
            }
            void apply(const ubyteArray& text) override
            {
                count += text.size();
            }
            void apply(const ushortArray& text) override
            {
                count += text.size();
            }
            void apply(const uintArray& text) override
            {
                count += text.size();
            }
        };

        std::map<ref_ptr<Font>, size_t> fontTextQuadCountMap;
        for(auto& text : children)
        {
            fontTextQuadCountMap[text->font] += vsg::visit<CountQuads>(text->text).count;
        }

        std::map<ref_ptr<Font>, TextQuads> fontTextQuadsMap;
        for(auto& [font, count] : fontTextQuadCountMap)
        {
            fontTextQuadsMap[font].reserve(count);
        }

        for(auto& text : children)
        {
            TextQuads& quads = fontTextQuadsMap[text->font];
            text->layout->layout(text->text, *(text->font), quads);
        }

        auto group = Group::create();
        for(auto& [font, quads] : fontTextQuadsMap)
        {
            if (auto subgraph = createRenderingSubgraph(font, quads, minimumAllocation))
            {
                group->addChild(subgraph);
            }
        }

        renderSubgraph = group;
    }
    else
    {
        for(auto& text : children)
        {
            text->setup(minimumAllocation);
        }
    }
}

ref_ptr<Node> TextGroup::createRenderingSubgraph(ref_ptr<Font> font, TextQuads& quads, uint32_t minimumAllocation)
{
    if (quads.empty())
    {
        return {};
    }

    ref_ptr<vec3Array> vertices;
    ref_ptr<vec4Array> colors;
    ref_ptr<vec4Array> outlineColors;
    ref_ptr<floatArray> outlineWidths;
    ref_ptr<vec3Array> texcoords;
    ref_ptr<Data> indices;
    ref_ptr<DrawIndexed> drawIndexed;

    ref_ptr<BindVertexBuffers> bindVertexBuffers;
    ref_ptr<BindIndexBuffer> bindIndexBuffer;
    ref_ptr<StateGroup> scenegraph;

    vec4 color = quads.front().colors[0];
    vec4 outlineColor = quads.front().outlineColors[0];
    float outlineWidth = quads.front().outlineWidths[0];
    bool singleColor = true;
    bool singleOutlineColor = true;
    bool singleOutlineWidth = true;
    for (auto& quad : quads)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (quad.colors[i] != color) singleColor = false;
            if (quad.outlineColors[i] != outlineColor) singleOutlineColor = false;
            if (quad.outlineWidths[i] != outlineWidth) singleOutlineWidth = false;
        }
    }


    uint32_t num_quads = std::max(static_cast<uint32_t>(quads.size()), minimumAllocation);
    uint32_t num_vertices = num_quads * 4;
    uint32_t num_colors = singleColor ? 1 : num_vertices;
    uint32_t num_outlineColors = singleOutlineColor ? 1 : num_vertices;
    uint32_t num_outlineWidths = singleOutlineWidth ? 1 : num_vertices;

    if (!vertices || num_vertices > vertices->size()) vertices = vec3Array::create(num_vertices);
    if (!colors || num_colors > colors->size()) colors = vec4Array::create(num_colors);
    if (!outlineColors || num_outlineColors > outlineColors->size()) outlineColors = vec4Array::create(num_outlineColors);
    if (!outlineWidths || num_outlineWidths > outlineWidths->size()) outlineWidths = floatArray::create(num_outlineWidths);
    if (!texcoords || num_vertices > texcoords->size()) texcoords = vec3Array::create(num_vertices);

    uint32_t vi = 0;

    float leadingEdgeGradient = 0.1f;

    if (singleColor) colors->set(0, color);
    if (singleOutlineColor) outlineColors->set(0, outlineColor);
    if (singleOutlineWidth) outlineWidths->set(0, outlineWidth);

    for (auto& quad : quads)
    {
        float leadingEdgeTilt = length(quad.vertices[0] - quad.vertices[1]) * leadingEdgeGradient;
        float topEdgeTilt = leadingEdgeTilt;

        vertices->set(vi, quad.vertices[0]);
        vertices->set(vi + 1, quad.vertices[1]);
        vertices->set(vi + 2, quad.vertices[2]);
        vertices->set(vi + 3, quad.vertices[3]);

        if (!singleColor)
        {
            colors->set(vi, quad.colors[0]);
            colors->set(vi + 1, quad.colors[1]);
            colors->set(vi + 2, quad.colors[2]);
            colors->set(vi + 3, quad.colors[3]);
        }

        if (!singleOutlineColor)
        {
            outlineColors->set(vi, quad.outlineColors[0]);
            outlineColors->set(vi + 1, quad.outlineColors[1]);
            outlineColors->set(vi + 2, quad.outlineColors[2]);
            outlineColors->set(vi + 3, quad.outlineColors[3]);
        }

        if (!singleOutlineWidth)
        {
            outlineWidths->set(vi, quad.outlineWidths[0]);
            outlineWidths->set(vi + 1, quad.outlineWidths[1]);
            outlineWidths->set(vi + 2, quad.outlineWidths[2]);
            outlineWidths->set(vi + 3, quad.outlineWidths[3]);
        }

        texcoords->set(vi, vec3(quad.texcoords[0].x, quad.texcoords[0].y, leadingEdgeTilt + topEdgeTilt));
        texcoords->set(vi + 1, vec3(quad.texcoords[1].x, quad.texcoords[1].y, topEdgeTilt));
        texcoords->set(vi + 2, vec3(quad.texcoords[2].x, quad.texcoords[2].y, 0.0f));
        texcoords->set(vi + 3, vec3(quad.texcoords[3].x, quad.texcoords[3].y, leadingEdgeTilt));

        vi += 4;
    }

    uint32_t num_indices = num_quads * 6;
    if (!indices || num_indices > indices->valueCount())
    {
        if (num_vertices > 65536) // check if requires uint or ushort indices
        {
            auto ui_indices = uintArray::create(num_indices);
            indices = ui_indices;

            auto itr = ui_indices->begin();
            vi = 0;
            for (uint32_t i = 0; i < num_quads; ++i)
            {
                (*itr++) = vi;
                (*itr++) = vi + 1;
                (*itr++) = vi + 2;
                (*itr++) = vi + 2;
                (*itr++) = vi + 3;
                (*itr++) = vi;

                vi += 4;
            }
        }
        else
        {
            auto us_indices = ushortArray::create(num_indices);
            indices = us_indices;

            auto itr = us_indices->begin();
            vi = 0;
            for (uint32_t i = 0; i < num_quads; ++i)
            {
                (*itr++) = vi;
                (*itr++) = vi + 1;
                (*itr++) = vi + 2;
                (*itr++) = vi + 2;
                (*itr++) = vi + 3;
                (*itr++) = vi;

                vi += 4;
            }
        }
    }

    if (!drawIndexed)
        drawIndexed = DrawIndexed::create(static_cast<uint32_t>(quads.size() * 6), 1, 0, 0, 0);
    else
        drawIndexed->indexCount = static_cast<uint32_t>(quads.size() * 6);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    if (!scenegraph)
    {
        scenegraph = StateGroup::create();

        auto shaderSet = createTextShaderSet(font->options);
        auto config = vsg::GraphicsPipelineConfig::create(shaderSet);

        auto& sharedObjects = font->sharedObjects;
        if (!sharedObjects && font->options) sharedObjects = font->options->sharedObjects;
        if (!sharedObjects) sharedObjects = SharedObjects::create();

        DataList arrays;
        config->assignArray(arrays, "inPosition", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
        config->assignArray(arrays, "inColor", singleColor ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, colors);
        config->assignArray(arrays, "inOutlineColor", singleOutlineColor ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, outlineColors);
        config->assignArray(arrays, "inOutlineWidth", singleOutlineWidth ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, outlineWidths);
        config->assignArray(arrays, "inTexCoord", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

        // set up sampler for atlas.
        auto sampler = Sampler::create();
        sampler->magFilter = VK_FILTER_LINEAR;
        sampler->minFilter = VK_FILTER_LINEAR;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 16.0f;
        sampler->maxLod = 12.0;

        if (sharedObjects) sharedObjects->share(sampler);

        Descriptors descriptors;
        config->assignTexture(descriptors, "textureAtlas", font->atlas, sampler);
        if (sharedObjects) sharedObjects->share(descriptors);

        // disable face culling so text can be seen from both sides
        config->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        // set alpha blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;

        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        config->colorBlendState->attachments = {colorBlendAttachment};

        if (sharedObjects)
            sharedObjects->share(config, [](auto gpc) { gpc->init(); });
        else
            config->init();

        scenegraph->add(config->bindGraphicsPipeline);

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(config->descriptorBindings);
        if (sharedObjects) sharedObjects->share(descriptorSetLayout);
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, descriptorSet);
        if (sharedObjects) sharedObjects->share(bindDescriptorSet);

        scenegraph->add(bindDescriptorSet);

        bindVertexBuffers = BindVertexBuffers::create(0, arrays);
        bindIndexBuffer = BindIndexBuffer::create(indices);

        // setup geometry
        auto drawCommands = Commands::create();
        drawCommands->addChild(bindVertexBuffers);
        drawCommands->addChild(bindIndexBuffer);
        drawCommands->addChild(drawIndexed);

        scenegraph->addChild(drawCommands);
    }
    else
    {
        info("TODO : CpuLayoutTechnique::setup(), does not yet support updates. Consider using GpuLayoutTechnique instead.");
        // bindVertexBuffers->copyDataToBuffers();
        // bindIndexBuffer->copyDataToBuffers();
    }

    return scenegraph;
}
