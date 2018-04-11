#include "OnDiskIndex.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

#include "Utils.h"

OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname) {
    const uint8_t *data = disk_map.data();
    uint32_t magic = *(uint32_t *) &data[0];
    uint32_t version = *(uint32_t *) &data[4];
    ntype = static_cast<IndexType>(*(uint32_t *) &data[8]);
    uint32_t reserved = *(uint32_t *) &data[12];

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (version != OnDiskIndex::VERSION) {
        throw std::runtime_error("unsupported version");
    }

    if (ntype != GRAM3) {
        throw std::runtime_error("invalid index type");
    }

    run_offsets = (uint32_t*) &data[disk_map.size() - (NUM_TRIGRAMS + 1) * 4];
}

std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram + 1];

    if (ptr == next_ptr) {
        std::cout << "empty index for " << trigram << std::endl;
    } else {
        std::cout << "for " << trigram << ": " << (next_ptr - ptr) << std::endl;
    }

    const uint8_t *data = disk_map.data();
    std::vector<FileId> out = read_compressed_run(&data[ptr], &data[next_ptr]);
    std::cout << "returning " << out.size() << " elems" << std::endl;
    return out;
}

void OnDiskIndex::on_disk_merge(std::string fname, IndexType merge_type, std::vector<OnDiskIndex> indexes) {
    std::ofstream out(fname, std::ofstream::binary);

    if (!std::all_of(indexes.begin(), indexes.end(), [merge_type](OnDiskIndex &ndx) {
        return ndx.ntype == merge_type;
    })) {
        throw new std::runtime_error("Unexpected index type during merge");
    }

    uint32_t magic = DB_MAGIC;
    uint32_t version = 5;
    uint32_t ndx_type = merge_type;
    uint32_t reserved = 0;

    out.write((char *) &magic, 4);
    out.write((char *) &version, 4);
    out.write((char *) &ndx_type, 4);
    out.write((char *) &reserved, 4);

    std::vector<uint32_t> out_offsets(NUM_TRIGRAMS + 1);
    std::vector<uint32_t> in_offsets(indexes.size());

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        out_offsets[i] = (uint32_t) out.tellp();
        std::vector<FileId> all_ids;
        for (int j = 0; j < indexes.size(); j++) {
            std::vector<FileId> new_ids = read_compressed_run(
                indexes[j].data() + indexes[j].run_offsets[i],
                indexes[j].data() + indexes[j].run_offsets[i + 1]);
            all_ids.insert(std::end(all_ids), std::begin(new_ids), std::end(new_ids));
        }
        compress_run(all_ids, out);
    }
    out_offsets[NUM_TRIGRAMS] = (uint32_t) out.tellp();

    out.write((char *) out_offsets.data(), (NUM_TRIGRAMS + 1) * 4);
    out.close();
}
