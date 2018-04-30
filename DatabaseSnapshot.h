#pragma once

#include <experimental/filesystem>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "Query.h"
#include "Task.h"

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

// Represents immutable snapshot of database state.
// Should never change, regardless of changes in real database.
class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::vector<const OnDiskDataset *> datasets;
    std::map<uint64_t, Task> tasks;
    size_t max_memory_size;

    std::string allocate_name() const;

  public:
    DatabaseSnapshot(
            fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset *> datasets,
            const std::map<uint64_t, std::unique_ptr<Task>> &tasks, size_t max_memory_size);
    void index_path(Task *task, const std::vector<IndexType> types, const std::string &filepath) const;
    void execute(const Query &query, Task *task, std::vector<std::string> *out) const;
    void compact(Task *task) const;
    const std::vector<const OnDiskDataset *> &get_datasets() const { return datasets; };
    const std::map<uint64_t, Task> &get_tasks() const { return tasks; };
};