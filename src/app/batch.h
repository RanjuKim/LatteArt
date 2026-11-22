// Owner: Cecelia Hein
// Usage: --batch <labels.csv> <photo_dir> <out.csv>
// Preferred UTF-8 CSV header: file,pattern,reference,notes
// - file: relative to photo_dir (or absolute).
// - reference: relative to the labels CSV directory (or absolute).
// - pattern: display/catalog name; no photo-wide target count is required.
// - notes: optional; CSV quoting supports commas, quotes and newlines.
// Legacy target_layers can select data/reference/<pattern><target_layers>.jpg
// when reference is empty/absent. Legacy true_layers is ignored for scoring.
// Writes out.csv, out_lines.csv and out_matches.csv, joined by row_id/line_index.
// Detail rows include direction/coordinate. Summary scores give each direction
// 50% weight, regardless of its valid-line count; quality Q is shared.
// Rejected/error rows have blank scores and no detail rows; errors are recorded
// per image, and processing continues. Invalid CSV or output failures throw.
#pragma once
namespace la::app {
int runBatch(int argc, char** argv);
} // namespace la::app
