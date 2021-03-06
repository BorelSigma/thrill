/*******************************************************************************
 * thrill/api/all_gather.hpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 * Copyright (C) 2015 Sebastian Lamm <seba.lamm@gmail.com>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#ifndef THRILL_API_ALL_GATHER_HEADER
#define THRILL_API_ALL_GATHER_HEADER

#include <thrill/api/action_node.hpp>
#include <thrill/api/dia.hpp>

#include <vector>

namespace thrill {
namespace api {

/*!
 * \ingroup api_layer
 */
template <typename ValueType>
class AllGatherNode final : public ActionNode
{
    static constexpr bool debug = false;

public:
    using Super = ActionNode;
    using Super::context_;

    template <typename ParentDIA>
    AllGatherNode(const ParentDIA& parent,
                  std::vector<ValueType>* out_vector)
        : ActionNode(parent.ctx(), "AllGather",
                     { parent.id() }, { parent.node() }),
          parent_stack_empty_(ParentDIA::stack_empty),
          out_vector_(out_vector)
    {
        auto pre_op_function = [this](const ValueType& input) {
                                   PreOp(input);
                               };

        // close the function stack with our pre op and register it at parent
        // node for output
        auto lop_chain = parent.stack().push(pre_op_function).fold();
        parent.node()->AddChild(this, lop_chain);
    }

    void StartPreOp(size_t /* id */) final {
        emitters_ = stream_->GetWriters();
    }

    void PreOp(const ValueType& element) {
        for (size_t i = 0; i < emitters_.size(); i++) {
            emitters_[i].Put(element);
        }
    }

    bool OnPreOpFile(const data::File& file, size_t /* parent_index */) final {
        if (!parent_stack_empty_) return false;
        for (size_t i = 0; i < emitters_.size(); i++) {
            emitters_[i].AppendBlocks(file.blocks());
        }
        return true;
    }

    void StopPreOp(size_t /* id */) final {
        // data has been pushed during pre-op -> close emitters
        for (size_t i = 0; i < emitters_.size(); i++) {
            emitters_[i].Close();
        }
    }

    //! Closes the output file
    void Execute() final {
        auto reader = stream_->GetCatReader(/* consume */ true);
        while (reader.HasNext()) {
            out_vector_->push_back(reader.template Next<ValueType>());
        }
    }

private:
    //! Whether the parent stack is empty
    const bool parent_stack_empty_;

    //! Vector pointer to write elements to.
    std::vector<ValueType>* out_vector_;

    data::CatStreamPtr stream_ { context_.GetNewCatStream(this) };
    std::vector<data::CatStream::Writer> emitters_;
};

template <typename ValueType, typename Stack>
std::vector<ValueType> DIA<ValueType, Stack>::AllGather() const {
    assert(IsValid());

    using AllGatherNode = api::AllGatherNode<ValueType>;

    std::vector<ValueType> output;

    auto node = common::MakeCounting<AllGatherNode>(*this, &output);

    node->RunScope();

    return output;
}

template <typename ValueType, typename Stack>
void DIA<ValueType, Stack>::AllGather(std::vector<ValueType>* out_vector) const {
    assert(IsValid());

    using AllGatherNode = api::AllGatherNode<ValueType>;

    auto node = common::MakeCounting<AllGatherNode>(*this, out_vector);

    node->RunScope();
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_ALL_GATHER_HEADER

/******************************************************************************/
