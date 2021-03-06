/*******************************************************************************
 * thrill/api/write_binary.hpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#ifndef THRILL_API_WRITE_BINARY_HEADER
#define THRILL_API_WRITE_BINARY_HEADER

#include <thrill/api/action_node.hpp>
#include <thrill/api/context.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/common/string.hpp>
#include <thrill/core/file_io.hpp>
#include <thrill/data/block_sink.hpp>
#include <thrill/data/block_writer.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace thrill {
namespace api {

/*!
 * \ingroup api_layer
 */
template <typename ValueType>
class WriteBinaryNode final : public ActionNode
{
    static constexpr bool debug = false;

public:
    using Super = ActionNode;
    using Super::context_;

    template <typename ParentDIA>
    WriteBinaryNode(const ParentDIA& parent,
                    const std::string& path_out,
                    size_t max_file_size)
        : ActionNode(parent.ctx(), "WriteBinary",
                     { parent.id() }, { parent.node() }),
          out_pathbase_(path_out),
          max_file_size_(max_file_size)
    {
        sLOG << "Creating write node.";

        block_size_ = std::min(data::default_block_size,
                               common::RoundUpToPowerOfTwo(max_file_size));
        sLOG << "block_size_" << block_size_;

        auto pre_op_fn = [=](const ValueType& input) {
                             return PreOp(input);
                         };
        // close the function stack with our pre op and register it at parent
        // node for output
        auto lop_chain = parent.stack().push(pre_op_fn).fold();
        parent.node()->AddChild(this, lop_chain);
    }

    DIAMemUse PreOpMemUse() final {
        return data::default_block_size;
    }

    //! writer preop: put item into file, create files as needed.
    void PreOp(const ValueType& input) {
        stats_total_elements_++;

        if (!sink_) OpenNextFile();

        try {
            writer_->PutNoSelfVerify(input);
        }
        catch (data::FullException&) {
            // sink is full. flush it. and repeat, which opens new file.
            OpenNextFile();

            try {
                writer_->PutNoSelfVerify(input);
            }
            catch (data::FullException&) {
                throw std::runtime_error(
                          "Error in WriteBinary: "
                          "an item is larger than the file size limit");
            }
        }
    }

    //! Closes the output file
    void StopPreOp(size_t /* id */) final {
        sLOG << "closing file" << out_pathbase_;
        writer_.reset();
        sink_.reset();

        Super::logger_
            << "class" << "WriteBinaryNode"
            << "total_elements" << stats_total_elements_
            << "total_writes" << stats_total_writes_;
    }

    void Execute() final { }

private:
    //! Implements BlockSink class writing to files with size limit.
    class SysFileSink final : public data::BoundedBlockSink
    {
    public:
        SysFileSink(data::BlockPool& block_pool,
                    size_t local_worker_id,
                    const std::string& path, size_t max_file_size,
                    size_t& stats_total_elements,
                    size_t& stats_total_writes)
            : BlockSink(block_pool, local_worker_id),
              BoundedBlockSink(block_pool, local_worker_id, max_file_size),
              file_(core::SysFile::OpenForWrite(path)),
              stats_total_elements_(stats_total_elements),
              stats_total_writes_(stats_total_writes) { }

        void AppendPinnedBlock(const data::PinnedBlock& b) final {
            sLOG << "SysFileSink::AppendBlock()" << b;
            stats_total_writes_++;
            file_.write(b.data_begin(), b.size());
        }

        void AppendPinnedBlock(data::PinnedBlock&& b) final {
            return AppendPinnedBlock(b);
        }

        void AppendBlock(const data::Block& block) {
            return AppendPinnedBlock(block.PinWait(local_worker_id()));
        }

        void AppendBlock(data::Block&& block) {
            return AppendPinnedBlock(block.PinWait(local_worker_id()));
        }

        void Close() final {
            file_.close();
        }

    private:
        core::SysFile file_;
        size_t& stats_total_elements_;
        size_t& stats_total_writes_;
    };

    using Writer = data::BlockWriter<SysFileSink>;

    //! Base path of the output file.
    std::string out_pathbase_;

    //! File serial number for this worker
    size_t out_serial_ = 0;

    //! Maximum file size
    size_t max_file_size_;

    //! Block size used by BlockWriter
    size_t block_size_ = data::default_block_size;

    //! BlockSink which writes to an actual file
    std::unique_ptr<SysFileSink> sink_;

    //! BlockWriter to sink.
    std::unique_ptr<Writer> writer_;

    size_t stats_total_elements_ = 0;
    size_t stats_total_writes_ = 0;

    //! Function to create sink_ and writer_ for next file
    void OpenNextFile() {
        writer_.reset();
        sink_.reset();

        // construct path from pattern containing ### and $$$
        std::string out_path = core::FillFilePattern(
            out_pathbase_, context_.my_rank(), out_serial_++);

        sLOG << "OpenNextFile() out_path" << out_path;

        sink_ = std::make_unique<SysFileSink>(
            context_.block_pool(), context_.local_worker_id(),
            out_path, max_file_size_,
            stats_total_elements_, stats_total_writes_);

        writer_ = std::make_unique<Writer>(sink_.get(), block_size_);
    }
};

template <typename ValueType, typename Stack>
void DIA<ValueType, Stack>::WriteBinary(
    const std::string& filepath, size_t max_file_size) const {

    using WriteBinaryNode = api::WriteBinaryNode<ValueType>;

    auto node = common::MakeCounting<WriteBinaryNode>(
        *this, filepath, max_file_size);

    node->RunScope();
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_WRITE_BINARY_HEADER

/******************************************************************************/
