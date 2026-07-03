#include "include/crush/raw_reader.h"

#include <iostream>
#include <filesystem>
#include <iomanip>

namespace forgerib {

RawReader::RawReader(
	const char* raw_path,
	const char* data_name,
	const int run,
	const int crate,
	const int module,
	const int rate
)
: raw_path_(raw_path)
, data_name_(data_name)
, run_(run)
, crate_id_(crate)
, module_(module)
, rate_(rate) {
	handle_.open(RawFileName(), std::ios::binary);
}

RawReader::~RawReader() {
	if (handle_.is_open()) handle_.close();
}

void CorrectSpecialHeader(unsigned int *data) {     // skip error headers and special conditions
	unsigned int event_length = (data[0] >> 17) & 0x3fff;
	unsigned int header_length = (data[0] >> 12) & 0x1f;
	unsigned int trace_length = (data[3] >> 16) & 0x7fff;
	if (event_length == 4 && header_length == 12 && trace_length == 0) {
		data[0] &= 0xffff7fff;
	}

	if ((data[2] & 0x8000) == 0x8000) {
		data[2] &= 0xffff7fff;
	}
}


bool ValidateEvent(
	const unsigned int *data,
	int crate_id,
	int slot_id
) {
	int peek_crate = int((data[0] >> 8) & 0xf);
	int peek_slot = int((data[0] >> 4) & 0xf);
	if (crate_id != peek_crate || slot_id != peek_slot) {
		// fprintf(
		// 	stderr,
		// 	"\nWarning: Get bad header: %08x %08x %08x %08x\n",
		// 	data[0], data[1], data[2], data[3]
		// );
		// std::cerr << "Invalid crate " << peek_crate << ", or slot " << slot_id << "\n";
		return false;
	}
	unsigned int event_length = (data[0] >> 17) & 0x3fff;
	unsigned int header_length = (data[0] >> 12) & 0x1f;
	unsigned int trace_length = (data[3] >> 16) & 0x7fff;
	if ((event_length - header_length)*2 != trace_length) {
		// fprintf(
		// 	stderr,
		// 	"\nWarning: Get bad header: %08x %08x %08x %08x\n",
		// 	data[0], data[1], data[2], data[3]
		// );
		// std::cerr << "Invalid length: " << header_length << ", " << event_length << ", " << trace_length << "\n";
		return false;
	}
	return true;
}

bool ValidateEvents(
	std::ifstream &fin,
	int crate_id,
	int slot_id,
	int count = 5
) {
	auto origin_pos = fin.tellg();
	bool fail = false;
	unsigned int data[4];
	for (int i = 0; i < count && fin.good(); ++i) {
		fin.read((char*)data, 16);
		if (fin.gcount() < 16) {
			fail = true;
			break;
		}
		if (!ValidateEvent(data, crate_id, slot_id)) {
			fail = true;
			break;
		}
	}
	fin.seekg(origin_pos);
	return !fail;
}

size_t SearchGoodEvent(
	std::ifstream &fin,
	int crate_id,
	int slot_id
) {
	fin.seekg(4, std::ios::cur);
	size_t offset = 4;
	while (!ValidateEvents(fin, crate_id, slot_id)) {
		if (!fin.good()) return 0;
		fin.seekg(4, std::ios::cur);
		offset += 4;
		// printf("Jump %lu.\n", offset);
		// if (offset >= 100) exit(-1);
	}
	return offset;
}

int RawReader::Read(
	DecodeEvent *event,
	RawEnergySum **sum,
	RawQDC **qdc,
	RawExternalTime **ext,
	DecodeTraces *trace
) {
	// read header
	handle_.read((char*)&header_, sizeof(RawHeader));
	// touch the end of the file
	std::streamsize bytes = handle_.gcount();
	if (bytes != 16) return bytes;

	CorrectSpecialHeader(header_.data);

	if (!ValidateEvent(header_.data, crate_id_, module_+2)) {
		fprintf(
			stderr,
			"\nWarning: Get bad header at %llu: %08x %08x %08x %08x\n",
			(unsigned long long)handle_.tellg(),
			header_.data[0], header_.data[1], header_.data[2], header_.data[3]
		);
		int jump_bytes = SearchGoodEvent(handle_, crate_id_, module_+2);
		if (jump_bytes == 0) return 0;
		fprintf(stderr, "Warning: Restore data after jump %d bytes.\n", jump_bytes);
		handle_.read((char*)&header_, sizeof(RawHeader));
	}

	// get event length and increase offset
	int event_length = (header_.data[0] >> 17) & 0x3fff;
	// fill information
	event->module = (unsigned short)(((header_.data[0] >> 4) & 0xf) - 2);
	event->channel = header_.data[0] & 0xf;
	event->energy = header_.data[3] & 0xffff;
	// cfd
	event->cfd = 0.0;
	if (rate_ == 100) {
		bool cfd_force = (header_.data[2] >> 31) != 0;
		if (!cfd_force) {
			event->cfd = double((header_.data[2] >> 16) & 0x7fff);
			event->cfd = event->cfd / 32768.0 * 10.0;
		}
		event->cfd_valid = !cfd_force;
	} else if (rate_ == 250) {
		bool cfd_force = (header_.data[2] >> 31) != 0;
		if (!cfd_force) {
			unsigned int cfds = (header_.data[2] >> 30) & 0x1;
			event->cfd = double((header_.data[2] >> 16) & 0x3fff);
			event->cfd = (event->cfd / 16384.0 - cfds) * 4.0;
		}
		event->cfd_valid = !cfd_force;
	} else { 					//500
		unsigned int cfds = (header_.data[2] >> 29) & 0x7;
		bool cfd_force = cfds == 7;
		if (!cfd_force) {
			event->cfd = double((header_.data[2] >> 16) & 0x1fff);
			event->cfd = (event->cfd / 8192.0 + cfds - 1) * 2.0;
		}
		event->cfd_valid = !cfd_force;
	}
	// timestamp
	event->timestamp = int64_t(header_.data[2] & 0xffff) << 32;
	event->timestamp |= header_.data[1];
	if (rate_ == 250) event->timestamp *= 8;
	else event->timestamp *= 10;
	// time
	event->time = double(event->timestamp) + event->cfd;
	// used
	event->used = false;

	int header_length = (header_.data[0] >> 12) & 0x1f;
	// read body
	if (header_length > 4 && ((header_length-4)&1)==0 && ((header_length-4)>>4)==0) {
		handle_.read((char*)body_, (header_length-4)*4);
		const unsigned int *body_pos = body_;
		bytes += handle_.gcount();

		// check existence of energy sum
		if (sum) {
			if ((header_length - 4) & 0x4) {
				*sum = (RawEnergySum*)body_pos;
				body_pos += 4;
			} else {
				*sum = nullptr;
			}
		}

		// check existence of QDC
		if (qdc) {
			if ((header_length - 4) & 0x8) {
				// get QDC
				*qdc = (RawQDC*)body_pos;
				body_pos += 8;
			} else {
				*qdc = nullptr;
			}
		}

		// check existence of external timestamp
		if (ext) {
			if ((header_length - 4) & 0x2) {
				*ext = (RawExternalTime*)body_pos;
				body_pos += 2;
			} else {
				*ext = nullptr;
			}
		}
	} else {
		if (sum) *sum = nullptr;
		if (qdc) *qdc = nullptr;
		if (ext) *ext = nullptr;
	}

	// read traces
	int trace_length = (header_.data[3] >> 16) & 0x7fff;
	int trace_out_of_range = header_.data[3] >> 31;
	if (trace) {
		trace->out_of_range = trace_out_of_range == 1;
		if (trace_length > 1000) {
			trace->length = 1000;
			handle_.read((char*)(trace->samples), 2000);
			handle_.seekg((trace_length - 1000) * 2, std::ios_base::cur);
		} else {
			trace->length = trace_length;
			handle_.read((char*)(trace->samples), trace_length * 2);
		}
		bytes += trace_length * 2;
	} else {
		auto last_pos = handle_.tellg();
		handle_.seekg(trace_length*2, std::ios_base::cur);
		bytes += handle_.tellg() - last_pos;
	}

	if (bytes == event_length*4) return bytes;
	return -bytes;
}

size_t RawReader::FileSize() const {
	if (std::filesystem::exists(RawFileName().Data())) {
			return std::filesystem::file_size(RawFileName().Data());
	}
	return 0;
}

}