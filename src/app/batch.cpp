// Owner: Junkyu Kim
#include "app/batch.h"

#include "app/pipeline.h"
#include "core/todo.h"

namespace la::app {

int runBatch(int argc, char** argv) {
    (void)argc; (void)argv;
    LA_TODO("runBatch: parse labels.csv -> runPipeline per row -> write results CSV");
}

}  // namespace la::app
