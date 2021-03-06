/*******************************************************************************
 * misc/json2profile.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2016 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/common/cmdline_parser.hpp>
#include <thrill/common/json_logger.hpp>
#include <thrill/common/logger.hpp>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace thrill; // NOLINT
using common::EscapeHtml;

static inline uint32_t GetUint32(const rapidjson::Document& d, const char* key) {
    if (!d[key].IsUint()) return 0;
    return d[key].GetUint();
}

static inline uint64_t GetUint64(const rapidjson::Document& d, const char* key) {
    if (!d[key].IsUint64()) return 0;
    return d[key].GetUint64();
}

static inline double GetDouble(const rapidjson::Document& d, const char* key1) {
    if (!d[key1].IsDouble()) return 0;
    return d[key1].GetDouble();
}

static inline std::string GetString(const rapidjson::Document& d, const char* key1) {
    if (!d[key1].IsString()) return std::string();
    return d[key1].GetString();
}

static inline double GetDouble(const rapidjson::Document& d,
                               const char* key1, const char* key2) {
    if (!d[key1].IsObject()) return 0;
    if (!d[key1][key2].IsDouble()) return 0;
    return d[key1][key2].GetDouble();
}

/******************************************************************************/

//! common base class for all json events
class CEvent
{
public:
    uint64_t ts = 0;
    size_t host_rank = 0;

    CEvent() = default;

    explicit CEvent(const rapidjson::Document& d)
        : ts(GetUint64(d, "ts")),
          host_rank(GetUint64(d, "host_rank"))
    { }
};

// {"ts":1461143811707721,"host_rank":0,"class":"Cmdline","event":"start","program":"suffix_sorting","argv":["\/home\/kit\/iti\/re9207\/thrill\/build\/examples\/suffix_sorting\/suffix_sorting","-a","dc3","\/work\/kit\/iti\/re9207\/pizza-chili\/english.200MB"],"cmdline":"\/home\/kit\/iti\/re9207\/thrill\/build\/examples\/suffix_sorting\/suffix_sorting -a dc3 \/work\/kit\/iti\/re9207\/pizza-chili\/english.200MB"}

class CCmdline : public CEvent
{
public:
    std::string event;
    std::string program;

    explicit CCmdline(const rapidjson::Document& d)
        : CEvent(d),
          event(GetString(d, "event")),
          program(GetString(d, "program"))
    { }

    bool operator < (const CCmdline& o) const {
        return ts < o.ts;
    }
};

std::vector<CCmdline> c_Cmdline;

static inline
std::string GetProgramName() {
    if (c_Cmdline.size() == 0)
        return "<unknown>";
    else
        return c_Cmdline.front().program;
}

// {"ts":1460574124046450,"host_rank":0,"class":"NetManager","event":"profile","flow":{"tx_bytes":243,"rx_bytes":128,"tx_speed":159.995,"rx_speed":79.9973,"tx_per_host":[0,123,96,24],"rx_per_host":[0,44,44,40]},"data":{"tx_bytes":13658972,"rx_bytes":207441,"tx_speed":2.72665e+07,"rx_speed":277389,"tx_per_host":[0,20208,13618556,20208],"rx_per_host":[0,105042,51168,51231]},"tx_bytes":13659215,"rx_bytes":207569,"tx_speed":2.72666e+07,"rx_speed":277469}

class CNetManager : public CEvent
{
public:
    double tx_speed;
    double rx_speed;

    explicit CNetManager(const rapidjson::Document& d)
        : CEvent(d),
          tx_speed(GetDouble(d, "tx_speed")),
          rx_speed(GetDouble(d, "rx_speed"))
    { }

    bool operator < (const CNetManager& o) const {
        return ts < o.ts;
    }
};

std::vector<CNetManager> c_NetManager;

// {"ts":1460574124050859,"host_rank":0,"class":"MemProfile","event":"profile","total":268315062,"float":134157531,"base":134157531,"float_hlc":{"high":201075379,"low":134157531,"close":134157531},"base_hlc":{"high":201075379,"low":134157531,"close":134157531}}

class CMemProfile : public CEvent
{
public:
    double total;
    double float_;
    double base;

    explicit CMemProfile(const rapidjson::Document& d)
        : CEvent(d),
          total(GetDouble(d, "total")),
          float_(GetDouble(d, "float")),
          base(GetDouble(d, "base"))
    { }

    bool operator < (const CMemProfile& o) const {
        return ts < o.ts;
    }
};

std::vector<CMemProfile> c_MemProfile;

// {"ts":1460574124049310,"host_rank":0,"class":"BlockPool","event":"profile","total_blocks":108,"total_bytes":232718336,"max_total_bytes":232718336,"total_ram_bytes":5904098640,"ram_bytes":328040448,"pinned_blocks":105,"pinned_bytes":134643712,"unpinned_blocks":3,"unpinned_bytes":193396736,"swapped_blocks":0,"swapped_bytes":0,"max_pinned_blocks":133,"max_pinned_bytes":134643712,"writing_blocks":0,"writing_bytes":0,"reading_blocks":0,"reading_bytes":0,"rd_ops_total":0,"rd_bytes_total":0,"wr_ops_total":0,"wr_bytes_total":0,"rd_ops":0,"rd_bytes":0,"rd_speed":0,"wr_ops":0,"wr_bytes":0,"wr_speed":0,"disk_allocation":0}

class CBlockPool : public CEvent
{
public:
    uint64_t total_bytes;
    uint64_t ram_bytes;
    uint64_t reading_bytes;
    uint64_t writing_bytes;
    uint64_t pinned_bytes;
    uint64_t unpinned_bytes;
    uint64_t swapped_bytes;

    double rd_speed;
    double wr_speed;

    explicit CBlockPool(const rapidjson::Document& d)
        : CEvent(d),
          total_bytes(GetUint64(d, "total_bytes")),
          ram_bytes(GetUint64(d, "ram_bytes")),
          reading_bytes(GetUint64(d, "reading_bytes")),
          writing_bytes(GetUint64(d, "writing_bytes")),
          pinned_bytes(GetUint64(d, "pinned_bytes")),
          unpinned_bytes(GetUint64(d, "unpinned_bytes")),
          swapped_bytes(GetUint64(d, "swapped_bytes")),
          rd_speed(GetDouble(d, "rd_speed")),
          wr_speed(GetDouble(d, "wr_speed"))
    { }

    bool operator < (const CBlockPool& o) const {
        return ts < o.ts;
    }
};

std::vector<CBlockPool> c_BlockPool;

// {"ts":1460574124550846,"host_rank":0,"class":"LinuxProcStats","event":"profile","cpu_user":38.0531,"cpu_nice":0,"cpu_sys":9.35525,"cpu_idle":52.2124,"cpu_iowait":0.126422,"cpu_hardirq":0,"cpu_softirq":0.252845,"cpu_steal":0,"cpu_guest":0,"cpu_guest_nice":0,"cores_user":[39,43,38.6139,36.7347,31.3131,44.6809,35.6436,35.4167],"cores_nice":[0,0,0,0,0,0,0,0],"cores_sys":[8,6,9.90099,6.12245,8.08081,11.7021,8.91089,17.7083],"cores_idle":[53,51,51.4851,57.1429,60.6061,42.5532,55.4455,46.875],"cores_iowait":[0,0,0,0,0,0,0,0],"cores_hardirq":[0,0,0,0,0,0,0,0],"cores_softirq":[0,0,0,0,0,1.06383,0,0],"cores_steal":[0,0,0,0,0,0,0,0],"cores_guest":[0,0,0,0,0,0,0,0],"cores_guest_nice":[0,0,0,0,0,0,0,0],"pr_user":314.583,"pr_sys":79.1667,"pr_nthreads":21,"pr_vsize":4163137536,"pr_rss":207618048,"net_rx_bytes":35270346,"net_tx_bytes":70409212,"net_rx_pkts":19137,"net_tx_pkts":13483,"net_rx_speed":3.52696e+07,"net_tx_speed":7.04077e+07,"disks":{"sda":{"rd_ios":0,"rd_merged":0,"rd_bytes":0,"rd_time":0,"wr_ios":0,"wr_merged":0,"wr_bytes":0,"wr_time":0,"ios_progr":0,"total_time":0,"rq_time":0},"sdb":{"rd_ios":0,"rd_merged":0,"rd_bytes":0,"rd_time":0,"wr_ios":0,"wr_merged":0,"wr_bytes":0,"wr_time":0,"ios_progr":0,"total_time":0,"rq_time":0},"sdc":{"rd_ios":0,"rd_merged":0,"rd_bytes":0,"rd_time":0,"wr_ios":0,"wr_merged":0,"wr_bytes":0,"wr_time":0,"ios_progr":0,"total_time":0,"rq_time":0},"sdd":{"rd_ios":0,"rd_merged":0,"rd_bytes":0,"rd_time":0,"wr_ios":0,"wr_merged":0,"wr_bytes":0,"wr_time":0,"ios_progr":0,"total_time":0,"rq_time":0}},"diskstats":{"rd_ios":0,"rd_merged":0,"rd_bytes":0,"rd_time":0,"wr_ios":0,"wr_merged":0,"wr_bytes":0,"wr_time":0,"ios_progr":0,"total_time":0,"rq_time":0},"meminfo":{"total":25282797568,"free":9041248256,"buffers":224858112,"cached":15018115072,"swap_total":19918848,"swap_free":12840960,"swap_used":7077888,"mapped":14721024,"shmem":53248}}

class CLinuxProcStats : public CEvent
{
public:
    double cpu_user;
    double cpu_sys;
    double pr_rss;

    double net_tx_speed;
    double net_rx_speed;

    double diskstats_rd_bytes;
    double diskstats_wr_bytes;

    explicit CLinuxProcStats(const rapidjson::Document& d)
        : CEvent(d),
          cpu_user(GetDouble(d, "cpu_user")),
          cpu_sys(GetDouble(d, "cpu_sys")),
          pr_rss(GetDouble(d, "pr_rss")),
          net_tx_speed(GetDouble(d, "net_tx_speed")),
          net_rx_speed(GetDouble(d, "net_rx_speed")),
          diskstats_rd_bytes(GetDouble(d, "diskstats", "rd_bytes")),
          diskstats_wr_bytes(GetDouble(d, "diskstats", "wr_bytes"))
    { }

    bool operator < (const CLinuxProcStats& o) const {
        return ts < o.ts;
    }
};

std::vector<CLinuxProcStats> c_LinuxProcStats;

// {"ts":1461082911913689,"host_rank":0,"worker_rank":18,"id":452,"label":"Zip","class":"DIABase","event":"create","type":"DOp","parents":[447,449,451]}

class CDIABase : public CEvent
{
public:
    uint32_t id = 0;
    std::string label;
    std::string event;
    std::string type;

    CDIABase() = default;

    explicit CDIABase(const rapidjson::Document& d)
        : CEvent(d),
          id(GetUint32(d, "id")),
          label(GetString(d, "label")),
          event(GetString(d, "event")),
          type(GetString(d, "type"))
    { }

    bool operator < (const CDIABase& o) const {
        return std::tie(host_rank, id) < std::tie(o.host_rank, o.id);
    }

    friend std::ostream& operator << (std::ostream& os, const CDIABase& c) {
        return os << EscapeHtml(c.label) << '.' << c.id;
    }
};

std::vector<CDIABase> c_DIABase;
std::map<uint32_t, CDIABase> m_DIABase;

// {"ts":1461082940825906,"host_rank":0,"class":"Stream","event":"close","id":255,"type":"CatStream","dia_id":427,"worker_rank":16,"rx_net_items":0,"rx_net_bytes":1568,"rx_net_blocks":32,"tx_net_items":1333959,"tx_net_bytes":5337698,"tx_net_blocks":38,"rx_int_items":728178,"rx_int_bytes":2912712,"rx_int_blocks":36,"tx_int_items":119826,"tx_int_bytes":479304,"tx_int_blocks":39}

class CStream : public CEvent
{
public:
    std::string event;
    uint32_t id;
    uint32_t dia_id;
    uint32_t worker_rank;

    uint64_t rx_net_items;
    uint64_t tx_net_items;
    uint64_t rx_net_bytes;
    uint64_t tx_net_bytes;

    uint64_t rx_int_items;
    uint64_t tx_int_items;
    uint64_t rx_int_bytes;
    uint64_t tx_int_bytes;

    explicit CStream(const rapidjson::Document& d)
        : CEvent(d),
          event(GetString(d, "event")),
          id(GetUint32(d, "id")),
          dia_id(GetUint32(d, "dia_id")),
          worker_rank(GetUint32(d, "worker_rank")),
          rx_net_items(GetUint64(d, "rx_net_items")),
          tx_net_items(GetUint64(d, "tx_net_items")),
          rx_net_bytes(GetUint64(d, "rx_net_bytes")),
          tx_net_bytes(GetUint64(d, "tx_net_bytes")),
          rx_int_items(GetUint64(d, "rx_int_items")),
          tx_int_items(GetUint64(d, "tx_int_items")),
          rx_int_bytes(GetUint64(d, "rx_int_bytes")),
          tx_int_bytes(GetUint64(d, "tx_int_bytes"))
    { }

    bool operator < (const CStream& o) const {
        return std::tie(id, host_rank, worker_rank)
               < std::tie(o.id, o.host_rank, o.worker_rank);
    }

    static void DetailHtmlHeader(std::ostream& os) {
        os << "<tr>";
        os << "<th>id</th>";
        os << "<th>dia_id</th>";
        os << "<th>host_rank</th>";
        os << "<th>worker_rank</th>";
        os << "<th>rx_items</th>";
        os << "<th>tx_items</th>";
        os << "<th>rx_bytes</th>";
        os << "<th>tx_bytes</th>";
        os << "<th>rx_net_items</th>";
        os << "<th>tx_net_items</th>";
        os << "<th>rx_net_bytes</th>";
        os << "<th>tx_net_bytes</th>";
        os << "<th>rx_int_items</th>";
        os << "<th>tx_int_items</th>";
        os << "<th>rx_int_bytes</th>";
        os << "<th>tx_int_bytes</th>";
        os << "</tr>";
    }

    void DetailHtmlRow(std::ostream& os) const {
        os << "<tr>";
        os << "<td>" << id << "</td>";
        os << "<td>" << m_DIABase[dia_id] << "</td>";
        os << "<td>" << host_rank << "</td>";
        os << "<td>" << worker_rank << "</td>";
        os << "<td>" << rx_net_items + rx_int_items << "</td>";
        os << "<td>" << tx_net_items + tx_int_items << "</td>";
        os << "<td>" << rx_net_bytes + rx_int_bytes << "</td>";
        os << "<td>" << tx_net_bytes + tx_int_bytes << "</td>";
        os << "<td>" << rx_net_items << "</td>";
        os << "<td>" << tx_net_items << "</td>";
        os << "<td>" << rx_net_bytes << "</td>";
        os << "<td>" << tx_net_bytes << "</td>";
        os << "<td>" << rx_int_items << "</td>";
        os << "<td>" << tx_int_items << "</td>";
        os << "<td>" << rx_int_bytes << "</td>";
        os << "<td>" << tx_int_bytes << "</td>";
        os << "</tr>";
    }
};

std::vector<CStream> c_Stream;

class CStreamSummary
{
public:
    uint32_t id = 0;
    uint32_t dia_id = 0;

    uint64_t rx_net_items = 0;
    uint64_t tx_net_items = 0;
    uint64_t rx_net_bytes = 0;
    uint64_t tx_net_bytes = 0;

    uint64_t rx_int_items = 0;
    uint64_t tx_int_items = 0;
    uint64_t rx_int_bytes = 0;
    uint64_t tx_int_bytes = 0;

    void Initialize(const CStream& s) {
        id = s.id;
        dia_id = s.dia_id;
        rx_net_items = s.rx_net_items;
        tx_net_items = s.tx_net_items;
        rx_net_bytes = s.rx_net_bytes;
        tx_net_bytes = s.tx_net_bytes;

        rx_int_items = s.rx_int_items;
        tx_int_items = s.tx_int_items;
        rx_int_bytes = s.rx_int_bytes;
        tx_int_bytes = s.tx_int_bytes;
    }

    void Add(const CStream& s) {
        assert(id == s.id);
        assert(dia_id == s.dia_id);

        rx_net_items += s.rx_net_items;
        tx_net_items += s.tx_net_items;
        rx_net_bytes += s.rx_net_bytes;
        tx_net_bytes += s.tx_net_bytes;

        rx_int_items += s.rx_int_items;
        tx_int_items += s.tx_int_items;
        rx_int_bytes += s.rx_int_bytes;
        tx_int_bytes += s.tx_int_bytes;
    }

    static void DetailHtmlHeader(std::ostream& os) {
        os << "<tr>";
        os << "<th>id</th>";
        os << "<th>dia_id</th>";
        os << "<th>rx_items</th>";
        os << "<th>tx_items</th>";
        os << "<th>rx_bytes</th>";
        os << "<th>tx_bytes</th>";
        os << "<th>rx_net_items</th>";
        os << "<th>tx_net_items</th>";
        os << "<th>rx_net_bytes</th>";
        os << "<th>tx_net_bytes</th>";
        os << "<th>rx_int_items</th>";
        os << "<th>tx_int_items</th>";
        os << "<th>rx_int_bytes</th>";
        os << "<th>tx_int_bytes</th>";
        os << "</tr>";
    }

    void DetailHtmlRow(std::ostream& os) const {
        os << "<tr>";
        os << "<td>" << id << "</td>";
        os << "<td>" << m_DIABase[dia_id] << "</td>";
        os << "<td>" << rx_net_items + rx_int_items << "</td>";
        os << "<td>" << tx_net_items + tx_int_items << "</td>";
        os << "<td>" << rx_net_bytes + rx_int_bytes << "</td>";
        os << "<td>" << tx_net_bytes + tx_int_bytes << "</td>";
        os << "<td>" << rx_net_items << "</td>";
        os << "<td>" << tx_net_items << "</td>";
        os << "<td>" << rx_net_bytes << "</td>";
        os << "<td>" << tx_net_bytes << "</td>";
        os << "<td>" << rx_int_items << "</td>";
        os << "<td>" << tx_int_items << "</td>";
        os << "<td>" << rx_int_bytes << "</td>";
        os << "<td>" << tx_int_bytes << "</td>";
        os << "</tr>";
    }
};

// {"ts":1461082954074899,"host_rank":0,"class":"File","event":"close","id":2261,"dia_id":4,"items":0,"bytes":0}

class CFile : public CEvent
{
public:
    std::string event;
    uint32_t id;
    uint32_t dia_id;

    uint64_t items;
    uint64_t bytes;

    explicit CFile(const rapidjson::Document& d)
        : CEvent(d),
          event(GetString(d, "event")),
          id(GetUint32(d, "id")),
          dia_id(GetUint32(d, "dia_id")),
          items(GetUint64(d, "items")),
          bytes(GetUint64(d, "bytes"))
    { }

    bool operator < (const CFile& o) const {
        return std::tie(ts, id) < std::tie(o.ts, o.id);
    }

    static void DetailHtmlHeader(std::ostream& os) {
        os << "<tr>";
        os << "<th>ts</th>";
        os << "<th>dia_id</th>";
        os << "<th>id</th>";
        os << "<th>host_rank</th>";
        os << "<th>items</th>";
        os << "<th>bytes</th>";
        os << "</tr>";
    }

    void DetailHtmlRow(std::ostream& os) const {
        os << "<tr>";
        os << "<td>" << ts << "</th>";
        os << "<td>" << m_DIABase[dia_id] << "</th>";
        os << "<td>" << id << "</th>";
        os << "<td>" << host_rank << "</th>";
        os << "<td>" << items << "</th>";
        os << "<td>" << bytes << "</th>";
        os << "</tr>";
    }
};

std::vector<CFile> c_File;

// {"ts":1461144110172911,"host_rank":0,"worker_rank":27,"id":472,"label":"Sort","class":"StageBuilder","event":"pushdata-start","targets":[478,479]}

class CStageBuilder : public CEvent
{
public:
    uint32_t worker_rank;
    uint32_t id;
    std::string label;
    std::string event;

    explicit CStageBuilder(const rapidjson::Document& d)
        : CEvent(d),
          worker_rank(GetUint32(d, "worker_rank")),
          id(GetUint32(d, "id")),
          label(GetString(d, "label")),
          event(GetString(d, "event"))
    { }

    bool operator < (const CStageBuilder& o) const {
        return std::tie(ts, worker_rank, id)
               < std::tie(o.ts, o.worker_rank, o.id);
    }
};

std::vector<CStageBuilder> c_StageBuilder;

/******************************************************************************/

size_t s_num_events = 0;

void LoadJsonProfile(std::istream& in) {
    std::string line;

    while (std::getline(in, line)) {
        rapidjson::Document d;
        d.Parse<0>(line.c_str());
        if (!d["class"].IsString()) continue;

        std::string class_str = d["class"].GetString();

        ++s_num_events;

        if (class_str == "Cmdline") {
            c_Cmdline.emplace_back(d);
        }
        else if (class_str == "NetManager") {
            c_NetManager.emplace_back(d);
        }
        else if (class_str == "MemProfile") {
            c_MemProfile.emplace_back(d);
        }
        else if (class_str == "BlockPool") {
            c_BlockPool.emplace_back(d);
        }
        else if (class_str == "LinuxProcStats") {
            c_LinuxProcStats.emplace_back(d);
        }
        else if (class_str == "Stream") {
            c_Stream.emplace_back(d);
        }
        else if (class_str == "File") {
            c_File.emplace_back(d);
        }
        else if (class_str == "DIABase") {
            c_DIABase.emplace_back(d);

            const CDIABase& db = c_DIABase.back();
            if (m_DIABase.count(db.id) == 0)
                m_DIABase.insert(std::make_pair(db.id, db));
        }
        else if (class_str == "StageBuilder") {
            c_StageBuilder.emplace_back(d);
        }
        else {
            --s_num_events;
        }
    }
}

void ProcessJsonProfile() {

    // sort

    std::sort(c_Cmdline.begin(), c_Cmdline.end());
    std::sort(c_LinuxProcStats.begin(), c_LinuxProcStats.end());
    std::sort(c_NetManager.begin(), c_NetManager.end());
    std::sort(c_MemProfile.begin(), c_MemProfile.end());
    std::sort(c_BlockPool.begin(), c_BlockPool.end());
    std::sort(c_Stream.begin(), c_Stream.end());
    std::sort(c_File.begin(), c_File.end());
    std::sort(c_DIABase.begin(), c_DIABase.end());
    std::sort(c_StageBuilder.begin(), c_StageBuilder.end());

    // subtract overall minimum timestamp

    uint64_t min_ts =
        std::min(c_LinuxProcStats.front().ts, c_NetManager.front().ts);
    min_ts = std::min(min_ts, c_MemProfile.front().ts);
    min_ts = std::min(min_ts, c_BlockPool.front().ts);
    min_ts = std::min(min_ts, c_StageBuilder.front().ts);

    for (auto& c : c_Cmdline) c.ts -= min_ts;
    for (auto& c : c_LinuxProcStats) c.ts -= min_ts;
    for (auto& c : c_NetManager) c.ts -= min_ts;
    for (auto& c : c_MemProfile) c.ts -= min_ts;
    for (auto& c : c_BlockPool) c.ts -= min_ts;
    for (auto& c : c_Stream) c.ts -= min_ts;
    for (auto& c : c_File) c.ts -= min_ts;
    for (auto& c : c_DIABase) c.ts -= min_ts;
    for (auto& c : c_StageBuilder) c.ts -= min_ts;
}

/******************************************************************************/

template <typename Stats, typename Select>
common::JsonVerbatim
MakeSeries(const std::vector<Stats>& c_Stats, const Select& select) {

    auto where = [](const Stats&) { return true; };

    bool first = true;
    uint64_t ts_curr = 0;

    uint64_t ts_sum = 0;
    double value_sum = 0;
    size_t value_count = 0;

    std::ostringstream oss;
    oss << '[';
    for (const Stats& c : c_Stats) {
        if (!where(c)) continue;

        if (c.ts / 1000000 != ts_curr) {
            if (value_count) {
                ts_sum /= value_count;
                value_sum /= value_count;
                if (!first) oss << ',', first = false;
                oss << '[' << ts_sum / 1000 << ',' << value_sum << ']' << ',';
            }
            ts_sum = 0;
            value_sum = 0;
            value_count = 0;
            ts_curr = c.ts / 1000000;
        }

        ts_sum += c.ts;
        value_sum += select(c);
        ++value_count;
    }
    oss << ']';

    return common::JsonVerbatim(oss.str());
}

/******************************************************************************/

void AddStageLines(common::JsonLine& xAxis) {
    common::JsonLine plotLines = xAxis.arr("plotLines");
    for (const CStageBuilder& c : c_StageBuilder) {
        if (c.worker_rank != 0) continue;
        common::JsonLine o = plotLines.obj();
        o << "width" << 1
          << "value" << c.ts / 1000.0
          << "color" << "#888888";
        o.sub("label")
            << "text" << (c.label + "." + std::to_string(c.id) + " " + c.event);
    }
}

/******************************************************************************/

std::string PageMain() {
    std::ostringstream oss;

    oss << "<!DOCTYPE html>\n";
    oss << "<html lang=\"en\">\n";
    oss << "  <head>\n";
    oss << "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
    oss << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    oss << "    \n";
    oss << "    <script src=\"https://code.jquery.com/jquery-1.12.2.min.js\"></script>\n";
    oss << "    <script src=\"https://code.highcharts.com/highcharts.js\"></script>\n";
    oss << "    <script src=\"https://code.highcharts.com/modules/exporting.js\"></script>\n";
    oss << "    \n";
    oss << "    <style type=\"text/css\">\n";
    oss << "table.dataframe td { text-align: right }\n";
    oss << "    </style>\n";
    oss << "    \n";
    oss << "    <!-- SUPPORT FOR IE6-8 OF HTML5 ELEMENTS -->\n";
    oss << "    <!--[if lt IE 9]>\n";
    oss << "    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/html5shiv/3.7.3/html5shiv.min.js\"></script>\n";
    oss << "    <![endif]-->\n";
    oss << "    \n";
    oss << "    <title>" << EscapeHtml(GetProgramName()) << "</title>\n";
    oss << "  </head>\n";
    oss << "\n";
    oss << "<body>\n";
    oss << "  <div id=\"chart_ID\" class=\"chart\" style=\"min-width: 310px; height: 600px; margin: 0 auto\"></div>\n";
    oss << "  <script type=\"text/javascript\">\n";
    oss << "    $(document).ready(function() {\n";
    oss << "      $(chart_ID).highcharts({";

    common::JsonLine j(nullptr, oss);

    using BObj = common::JsonBeginObj;
    using EObj = common::JsonEndObj;

    j.sub("title")
        << "text" << GetProgramName();

    j.sub("chart")
        << "renderTo" << "chart"
        << "zoomType" << "x"
        << "panning" << true
        << "panKey" << "shift";
    {
        common::JsonLine xAxis = j.arr("xAxis");
        common::JsonLine x1 = xAxis.obj();
        x1 << "type" << "datetime"
           << BObj("title") << "text" << "Execution Time" << EObj();
        AddStageLines(x1);
    }
    {
        common::JsonLine yAxis = j.arr("yAxis");
        common::JsonLine y1 = yAxis.obj();
        y1.sub("title") << "text" << "CPU Load (%)";
        y1.Close();

        common::JsonLine y2 = yAxis.obj();
        y2.sub("title") << "text" << "Network/Disk (B/s)";
        y2 << "opposite" << true;
        y2.Close();

        common::JsonLine y3 = yAxis.obj();
        y3.sub("title") << "text" << "Data System (B)";
        y3 << "opposite" << true;
        y3.Close();
    }

    j.sub("legend")
        << "layout" << "vertical"
        << "align" << "right"
        << "verticalAlign" << "middle"
        << "borderWidth" << 0;

    {
        common::JsonLine po = j.sub("plotOptions");
        po.sub("series")
            << "animation" << 0
            << BObj("marker") << "radius" << 2.5 << EObj();
    }
    {
        common::JsonLine s = j.arr("series");

        // ProcStats

        s.obj()
            << "name" << "CPU"
            << BObj("tooltip") << "valueSuffix" << " %" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) {
                          return c.cpu_user + c.cpu_sys;
                      });

        s.obj()
            << "name" << "CPU User" << "visible" << false
            << BObj("tooltip") << "valueSuffix" << " %" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.cpu_user; });

        s.obj()
            << "name" << "CPU Sys" << "visible" << false
            << BObj("tooltip") << "valueSuffix" << " %" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.cpu_sys; });

        s.obj()
            << "name" << "Mem RSS" << "visible" << false << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.pr_rss; });

        // Network

        s.obj()
            << "name" << "TX+RX net" << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_NetManager,
                      [](const CNetManager& c) { return c.tx_speed + c.rx_speed; });

        s.obj()
            << "name" << "TX net" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_NetManager,
                      [](const CNetManager& c) { return c.tx_speed; });

        s.obj()
            << "name" << "RX net" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_NetManager,
                      [](const CNetManager& c) { return c.rx_speed; });

        s.obj()
            << "name" << "TX+RX sys net" << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats, [](const CLinuxProcStats& c) {
                          return c.net_tx_speed + c.net_rx_speed;
                      });

        s.obj()
            << "name" << "TX sys net" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.net_tx_speed; });

        s.obj()
            << "name" << "RX sys net" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.net_rx_speed; });

        // Disk

        s.obj()
            << "name" << "I/O sys" << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) {
                          return c.diskstats_rd_bytes + c.diskstats_wr_bytes;
                      });

        s.obj()
            << "name" << "I/O sys read" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.diskstats_rd_bytes; });

        s.obj()
            << "name" << "I/O sys write" << "visible" << false << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_LinuxProcStats,
                      [](const CLinuxProcStats& c) { return c.diskstats_wr_bytes; });

        // BlockPool

        s.obj()
            << "name" << "Data bytes" << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.total_bytes; });

        s.obj()
            << "name" << "RAM bytes" << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.ram_bytes; });

        s.obj()
            << "name" << "Reading bytes" << "visible" << false << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.reading_bytes; });

        s.obj()
            << "name" << "Writing bytes" << "visible" << false << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.writing_bytes; });

        s.obj()
            << "name" << "Pinned bytes" << "visible" << false << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.pinned_bytes; });

        s.obj()
            << "name" << "Unpinned bytes" << "visible" << false << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.unpinned_bytes; });

        s.obj()
            << "name" << "Swapped bytes" << "yAxis" << 2
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.swapped_bytes; });

        s.obj()
            << "name" << "I/O read" << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.rd_speed; });

        s.obj()
            << "name" << "I/O write" << "yAxis" << 1
            << BObj("tooltip") << "valueSuffix" << " B/s" << EObj()
            << "data"
            << MakeSeries(c_BlockPool,
                      [](const CBlockPool& c) { return c.wr_speed; });

        // MemProfile

        s.obj()
            << "name" << "Mem Total" << "yAxis" << 2 << "visible" << false
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_MemProfile,
                      [](const CMemProfile& c) { return c.total; });

        s.obj()
            << "name" << "Mem Float" << "yAxis" << 2 << "visible" << false
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_MemProfile,
                      [](const CMemProfile& c) { return c.float_; });

        s.obj()
            << "name" << "Mem Base" << "yAxis" << 2 << "visible" << false
            << BObj("tooltip") << "valueSuffix" << " B" << EObj()
            << "data"
            << MakeSeries(c_MemProfile,
                      [](const CMemProfile& c) { return c.base; });
    }

    oss << "      });\n";
    oss << "    });\n";
    oss << "  </script>\n";
    oss << "\n";

    /**************************************************************************/

    oss << "<h2>Stream Summary</h2>\n";

    oss << "<table border=\"1\" class=\"dataframe\">";
    oss << "<thead>";
    CStreamSummary::DetailHtmlHeader(oss);
    oss << "</thead>";
    oss << "<tbody>";
    {
        CStreamSummary ss;
        for (const CStream& c : c_Stream) {
            if (c.event != "close") continue;
            if (ss.id != c.id || ss.dia_id != c.dia_id) {
                if (ss.id != 0) {
                    ss.DetailHtmlRow(oss);
                }
                ss.Initialize(c);
            }
            else {
                ss.Add(c);
            }
        }
    }
    oss << "</tbody>";
    oss << "</table>";
    oss << "\n";

    /**************************************************************************/

    oss << "<h2>Stream Details</h2>\n";

    oss << "<table border=\"1\" class=\"dataframe\">";
    oss << "<thead>";
    CStream::DetailHtmlHeader(oss);
    oss << "</thead>";
    oss << "<tbody>";
    for (const CStream& c : c_Stream) {
        if (c.event != "close") continue;
        c.DetailHtmlRow(oss);
    }
    oss << "</tbody>";
    oss << "</table>";
    oss << "\n";

    /**************************************************************************/

    oss << "<h2>File Details</h2>\n";

    oss << "<table border=\"1\" class=\"dataframe\">";
    oss << "<thead>";
    CFile::DetailHtmlHeader(oss);
    oss << "</thead>";
    oss << "<tbody>";
    for (const CFile& c : c_File) {
        if (c.event != "close") continue;
        if (c.items == 0 && c.bytes == 0) continue;
        c.DetailHtmlRow(oss);
    }
    oss << "</tbody>";
    oss << "</table>";
    oss << "\n";

    /**************************************************************************/

    oss << "</body>\n";
    oss << "</html>\n";

    return oss.str();
}

/******************************************************************************/

int main(int argc, char* argv[]) {
    common::CmdlineParser clp;
    clp.SetDescription("Thrill Json Profile Parser");

    std::vector<std::string> inputs;
    clp.AddParamStringlist("inputs", inputs, "json inputs");

    if (!clp.Process(argc, argv)) return -1;

    for (const std::string& input : inputs)
    {
        std::ifstream in(input);
        LoadJsonProfile(in);
    }

    ProcessJsonProfile();

    std::cerr << "Parsed " << s_num_events << " events "
              << "from " << inputs.size() << " files" << std::endl;

    std::cout << PageMain();

    return 0;
}

/******************************************************************************/
