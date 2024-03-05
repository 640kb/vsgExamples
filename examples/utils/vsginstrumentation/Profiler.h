#pragma once

#include <vsg/utils/Instrumentation.h>
#include <vsg/ui/FrameStamp.h>
#include <vsg/io/stream.h>
#include <vsg/vk/Device.h>

namespace vsg
{
    class ProfileLog : public Inherit<Object, ProfileLog>
    {
    public:

        ProfileLog(size_t size = 16384);

        enum Type : uint8_t
        {
            NO_TYPE,
            FRAME,
            CPU,
            COMMAND_BUFFER,
            GPU
        };

        struct Entry
        {
            Type type = {};
            bool enter = true;
            vsg::time_point cpuTime = {};
            uint64_t gpuTime = 0;
            const SourceLocation* sourceLocation = nullptr;
            const Object* object = nullptr;
            uint64_t reference = 0;
            std::thread::id thread_id = {};
        };

        std::map<std::thread::id, std::string> threadNames;
        std::vector<Entry> entries;
        std::atomic_uint64_t index = 0;
        std::vector<uint64_t> frameIndices;
        double timestampScaleToMilliseconds = 1e-6;


        Entry& enter(uint64_t& reference, Type type)
        {
            reference = index.fetch_add(1);
            Entry& enter_entry = entry(reference);
            enter_entry.enter = true;
            enter_entry.type = type;
            enter_entry.reference = 0;
            enter_entry.cpuTime = clock::now();
            enter_entry.gpuTime = 0;
            enter_entry.thread_id = std::this_thread::get_id();
            return enter_entry;
        }

        Entry& leave(uint64_t& reference, Type type)
        {
            Entry& enter_entry = entry(reference);

            uint64_t new_reference = index.fetch_add(1);
            Entry& leave_entry = entry(new_reference);

            enter_entry.reference = new_reference;
            leave_entry.cpuTime = clock::now();
            leave_entry.gpuTime = 0;
            leave_entry.enter = false;
            leave_entry.type = type;
            leave_entry.reference = reference;
            leave_entry.thread_id = std::this_thread::get_id();
            reference = new_reference;
            return leave_entry;
        }

        Entry& entry(uint64_t reference)
        {
            return entries[reference % entries.size()];
        }

        void report(std::ostream& out);
        uint64_t report(std::ostream& out, uint64_t reference);

    public:
        void read(Input& input) override;
        void write(Output& output) const override;
    };
    VSG_type_name(ProfileLog)

    class Profiler : public Inherit<Instrumentation, Profiler>
    {
    public:

        struct Settings : public Inherit<Object, Settings>
        {
            unsigned int cpu_instrumentation_level = 1;
            unsigned int gpu_instrumentation_level = 1;
            uint32_t log_size = 16384;
            uint32_t gpu_timestamp_size = 1024;
        };

        Profiler(ref_ptr<Settings> in_settings = {});

        ref_ptr<Settings> settings;
        mutable ref_ptr<ProfileLog> log;

        /// resources for collecting GPU stats for a single device on a single frame
        struct GPUStatsCollection : public Inherit<Object, GPUStatsCollection>
        {
            ref_ptr<Device> device;
            mutable ref_ptr<QueryPool> queryPool;
            mutable std::atomic<uint32_t> queryIndex = 0;
            std::vector<uint64_t> references;
            std::vector<uint64_t> timestamps;
        };

        /// resources for collecting GPU stats for all devices for a single frame
        struct FrameStatsCollection
        {
            FrameStatsCollection() : perDeviceGpuStats(VSG_MAX_DEVICES) {}

            std::vector<ref_ptr<GPUStatsCollection>> perDeviceGpuStats;
        };

        mutable size_t frameIndex = 0;
        mutable std::vector<FrameStatsCollection> perFrameGPUStats;

        void writeGpuTimestamp(CommandBuffer& commandBuffer, uint64_t reference, VkPipelineStageFlagBits pipelineStage) const;
        VkResult getGpuResults(FrameStatsCollection& frameStats) const;

    public:
        void setThreadName(const std::string& /*name*/) const override;

        void enterFrame(const SourceLocation* /*sl*/, uint64_t& /*reference*/, FrameStamp& /*frameStamp*/) const override;
        void leaveFrame(const SourceLocation* /*sl*/, uint64_t& /*reference*/, FrameStamp& /*frameStamp*/) const override;

        void enter(const SourceLocation* /*sl*/, uint64_t& /*reference*/, const Object* /*object*/ = nullptr) const override;
        void leave(const SourceLocation* /*sl*/, uint64_t& /*reference*/, const Object* /*object*/ = nullptr) const override;

        void enterCommandBuffer(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/) const override;
        void leaveCommandBuffer(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/) const override;

        void enter(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/, const Object* /*object*/ = nullptr) const override;
        void leave(const SourceLocation* /*sl*/, uint64_t& /*reference*/, CommandBuffer& /*commandBuffer*/, const Object* /*object*/ = nullptr) const override;

        void finish();
    };
    VSG_type_name(Profiler)
}
