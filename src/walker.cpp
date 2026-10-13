#include "swaggercpp/walker.hpp"

#include "detail/document_walk.hpp"

namespace swaggercpp {

void DocumentWalker::walk(const Document& document, DocumentVisitor& visitor) {
    detail::DocumentTraversal<false, DocumentVisitor>(visitor).walk(document);
}

} // namespace swaggercpp
