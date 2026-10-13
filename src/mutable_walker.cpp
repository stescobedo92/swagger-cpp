#include "swaggercpp/mutable_walker.hpp"

#include "detail/document_walk.hpp"

namespace swaggercpp {

void MutableDocumentWalker::walk(Document& document, MutableDocumentVisitor& visitor) {
    detail::DocumentTraversal<true, MutableDocumentVisitor>(visitor).walk(document);
}

} // namespace swaggercpp
