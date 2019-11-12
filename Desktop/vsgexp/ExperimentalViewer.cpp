#include "ExperimentalViewer.h"

using namespace vsg;

CommandGraph::CommandGraph()
{
#if 0
    ref_ptr<CommandPool> cp = CommandPool::create(_device, _physicalDevice->getGraphicsFamily());
    commandBuffer = CommandBuffer::create(_device, cp, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
#endif
}

void CommandGraph::accept(DispatchTraversal& dispatchTraversal) const
{
    // from DispatchTraversal index?
    uint32_t index = 0;

    // or select index when maps to a dormant CommandBuffer
    VkCommandBuffer vk_commandBuffer = *(commandBuffers[index]);

    // need to set up the command
    // if we are nested within a CommandBuffer already then use VkCommandBufferInheritanceInfo
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(vk_commandBuffer, &beginInfo);

    // traverse the command graph
    traverse(dispatchTraversal);

    vkEndCommandBuffer(vk_commandBuffer);
}

RenderGraph::RenderGraph()
{
#if 0
    renderPass; // provided by window
    framebuffer // provided by window/swapchain
    renderArea; // provide by camera?
    clearValues; // provide by camera?
#endif
}


void RenderGraph::accept(DispatchTraversal& dispatchTraversal) const
{
    if (camera)
    {
        dmat4 projMatrix, viewMatrix;
        camera->getProjectionMatrix()->get(projMatrix);
        camera->getViewMatrix()->get(viewMatrix);

        dispatchTraversal.setProjectionAndViewMatrix(projMatrix, viewMatrix);
    }

    // TODO get next window index from State? DispatchVisitor?
    uint32_t index = 0;

    VkCommandBuffer vk_commandBuffer = *(dispatchTraversal.state->_commandBuffer);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = *renderPass;
    renderPassInfo.framebuffer = *framebuffers[index];
    renderPassInfo.renderArea = renderArea;

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(vk_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // traverse the command buffer to place the commands into the command buffer.
    traverse(dispatchTraversal);

    vkCmdEndRenderPass(vk_commandBuffer);
}


VkResult Submission::submit(ref_ptr<FrameStamp> frameStamp)
{
    std::vector<VkSemaphore> vk_waitSemaphores;
    std::vector<VkPipelineStageFlags> vk_waitStages;
    std::vector<VkCommandBuffer> vk_commandBuffers;
    std::vector<VkSemaphore> vk_signalSemaphores;

    if (fence)
    {
        uint64_t timeout = 10000000000;
        VkResult result = VK_SUCCESS;
        while ((result = fence->wait(timeout)) == VK_TIMEOUT)
        {
            std::cout << "Submission::submit(ref_ptr<FrameStamp> frameStamp)) fence->wait(" << timeout << ") failed with result = " << result << std::endl;
            //exit(1);
            //throw "Window::populateCommandBuffers(uint32_t index, ref_ptr<vsg::FrameStamp> frameStamp) timeout";
        }

        for (auto& semaphore : fence->dependentSemaphores())
        {
            //std::cout<<"Window::populateCommandBuffers(..) "<<*(semaphore->data())<<" "<<semaphore->numDependentSubmissions().load()<<std::endl;
            semaphore->numDependentSubmissions().exchange(0);
        }

        fence->dependentSemaphores().clear();
        fence->reset();
    }

    for(auto& semaphore : waitSemaphores)
    {
        vk_waitSemaphores.emplace_back(*(semaphore));
        vk_waitStages.emplace_back(semaphore->pipelineStageFlags());
    }

    // need to compute from scene graph
    uint32_t _maxSlot = 2;
    uint32_t index = 0;
    DatabasePager* databasePager = nullptr;

    for(auto& commandGraph : commandGraphs)
    {
        // pass the inext to dispatchTraversal visitor?  Only for RenderGraph?
        DispatchTraversal dispatchTraversal(commandGraph->commandBuffers[index], _maxSlot, frameStamp);

        dispatchTraversal.databasePager = databasePager;
        if (databasePager) dispatchTraversal.culledPagedLODs = databasePager->culledPagedLODs;

        commandGraph->accept(dispatchTraversal);
    }

    if (databasePager)
    {
        for (auto& semaphore : databasePager->getSemaphores())
        {
            if (semaphore->numDependentSubmissions().load() > 1)
            {
                std::cout << "    Warning: Viewer::submitNextFrame() waitSemaphore " << *(semaphore->data()) << " " << semaphore->numDependentSubmissions().load() << std::endl;
            }
            else
            {
                // std::cout<<"    Viewer::submitNextFrame() waitSemaphore "<<*(semaphore->data())<<" "<<semaphore->numDependentSubmissions().load()<<std::endl;
            }

            vk_waitSemaphores.emplace_back(*semaphore);
            vk_waitStages.emplace_back(semaphore->pipelineStageFlags());

            semaphore->numDependentSubmissions().fetch_add(1);
            fence->dependentSemaphores().emplace_back(semaphore);
        }
    }


    for(auto& semaphore : signalSemaphores)
    {
        vk_signalSemaphores.emplace_back(*(semaphore));
    }


    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_waitSemaphores.size());
    submitInfo.pWaitSemaphores = vk_waitSemaphores.data();
    submitInfo.pWaitDstStageMask = vk_waitStages.data();

    submitInfo.commandBufferCount = static_cast<uint32_t>(vk_commandBuffers.size());
    submitInfo.pCommandBuffers = vk_commandBuffers.data();

    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vk_signalSemaphores.size());
    submitInfo.pSignalSemaphores = vk_signalSemaphores.data();

    return queue->submit(submitInfo, fence);
}

VkResult Presentation::present()
{
    std::vector<VkSemaphore> vk_semaphores;
    for(auto& semaphore : waitSemaphores)
    {
        vk_semaphores.emplace_back(*(semaphore));
    }

    std::vector<VkSwapchainKHR> vk_swapchains;
    for(auto& swapchain : swapchains)
    {
        vk_swapchains.emplace_back(*(swapchain));
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_semaphores.size());
    presentInfo.pWaitSemaphores = vk_semaphores.data();
    presentInfo.swapchainCount = static_cast<uint32_t>(vk_swapchains.size());
    presentInfo.pSwapchains = vk_swapchains.data();
    presentInfo.pImageIndices = indices.data();

    return queue->present(presentInfo);
}

ExperimentalViewer::ExperimentalViewer()
{
}

ExperimentalViewer::~ExperimentalViewer()
{
    // TODO, need handle clean up of threads and Vulkan queues
}

void ExperimentalViewer::addWindow(ref_ptr<Window> window)
{
    // TODO, need to review PerDeviceObjects setup
    Viewer::addWindow(window);
}

bool ExperimentalViewer::advanceToNextFrame()
{
    // Probably OK
    return Viewer::advanceToNextFrame();
}

void ExperimentalViewer::advance()
{
    // Probably OK
    Viewer::advance();
}

void ExperimentalViewer::handleEvents()
{
    // Probably OK
    Viewer::handleEvents();
}

void ExperimentalViewer::reassignFrameCache()
{
    // TODO, need to review PerDeviceObjects
    Viewer::reassignFrameCache();
}

bool ExperimentalViewer::acquireNextFrame()
{
    // Probably OK
    return Viewer::acquireNextFrame();
}

bool ExperimentalViewer::populateNextFrame()
{
    // TODO, need to replace the Window::populate.. calls
    //       could just do nothing here, rely upon Submission to do the required update.
    return Viewer::populateNextFrame();
}

bool ExperimentalViewer::submitNextFrame()
{
    // TODO, need to replace the PerDeviceObjects / Window / Fence and Semaphore handles

    for(auto& submission : submissions)
    {
        submission.submit(_frameStamp);
    }

    if (presentation) presentation->present();

    return Viewer::submitNextFrame();
}

void ExperimentalViewer::compile(BufferPreferences bufferPreferences)
{
    // TODO, need to reorganize how Stage + command buffers are handled to collect stats or do final compile
    Viewer::compile(bufferPreferences);
}
