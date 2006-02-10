
#include "config.h"
#include "include/types.h"

//#define MDS_CACHE_SIZE        4*10000   -> <20mb
//#define MDS_CACHE_SIZE        80000         62mb

#define AVG_PER_INODE_SIZE    450
#define MDS_CACHE_MB_TO_INODES(x) ((x)*1000000/AVG_PER_INODE_SIZE)

//#define MDS_CACHE_SIZE       MDS_CACHE_MB_TO_INODES( 50 )
//#define MDS_CACHE_SIZE 1500000
#define MDS_CACHE_SIZE 150000


// hack hack hack ugly FIXME
#include "common/Mutex.h"
long buffer_total_alloc = 0;
Mutex bufferlock;



FileLayout g_OSD_FileLayout( 1<<20, 1, 1<<20, 2 );  // stripe over 1M objects, 2x replication
//FileLayout g_OSD_FileLayout( 1<<17, 4, 1<<20 );   // 128k stripes over sets of 4

// ??
FileLayout g_OSD_MDDirLayout( 1<<8, 1<<2, 1<<19, 3 );
//FileLayout g_OSD_MDDirLayout( 1<<20, 1, 1<<20, 2 );  // stripe over 1M objects, 2x replication

// stripe mds log over 128 byte bits (see mds_log_pad_entry below to match!)
FileLayout g_OSD_MDLogLayout( 1<<20, 1, 1<<20, 3 );  // new (good?) way
//FileLayout g_OSD_MDLogLayout( 1<<7, 32, 1<<20, 3 );  // new (good?) way
//FileLayout g_OSD_MDLogLayout( 57, 32, 1<<20 );  // pathological case to test striping buffer mapping
//FileLayout g_OSD_MDLogLayout( 1<<20, 1, 1<<20 );  // old way


md_config_t g_conf = {
  num_mds: 1,
  num_osd: 4,
  num_client: 1,

  mkfs: false,

  // profiling and debugging
  log: true,
  log_interval: 1,
  log_name: (char*)0,

  log_messages: true,
  log_pins: true,

  fake_clock: false,
  fakemessenger_serialize: true,
  fake_osdmap_expand: 0,

  debug: 1,
  debug_mds_balancer: 1,
  debug_mds_log: 1,
  debug_buffer: 0,
  debug_filer: 0,
  debug_client: 0,
  debug_osd: 0,
  debug_ebofs: 1,
  debug_bdev: 1,         // block device
  debug_ns: 0,
  
  tcp_skip_rank0: false,

  // --- client ---
  client_cache_size: 300,
  client_cache_mid: .5,
  client_cache_stat_ttl: 10, // seconds until cached stat results become invalid
  client_use_random_mds:  false,

  client_sync_writes: 0,

  client_bcache: 0,
  client_bcache_alloc_minsize: 1<<10, // 1KB
  client_bcache_alloc_maxsize: 1<<18, // 256KB
  client_bcache_ttl: 30, // seconds until dirty buffers are written to disk
  client_bcache_size: 2<<30, // 2GB
  //client_bcache_size: 5<<20, // 5MB
  client_bcache_lowater: 60, // % of size
  client_bcache_hiwater: 80, // % of size
  client_bcache_align: 1<<10, // 1KB splice alignment

  client_trace: 0,
  fuse_direct_io: 0,
  
  // --- mds ---
  mds_cache_size: MDS_CACHE_SIZE,
  mds_cache_mid: .7,

  mds_log: true,
  mds_log_max_len:  MDS_CACHE_SIZE / 3,
  mds_log_max_trimming: 5000,//25600,
  mds_log_read_inc: 1<<20,
  mds_log_pad_entry: 256,//64,
  mds_log_before_reply: true,
  mds_log_flush_on_shutdown: true,

  mds_bal_replicate_threshold: 8000,
  mds_bal_unreplicate_threshold: 1000,
  mds_bal_interval: 30,           // seconds
  mds_bal_idle_threshold: .1,
  mds_bal_max: -1,
  mds_bal_max_until: -1,

  mds_commit_on_shutdown: true,

  mds_verify_export_dirauth: true,


  // --- osd ---
  osd_pg_bits: 6,
  osd_max_rep: 4,
  osd_maxthreads: 10,    // 0 == no threading
  osd_mkfs: false,
  osd_fake_sync: false,
  
  fakestore_fake_sync: 2,    // 2 seconds
  fakestore_fsync: true,
  fakestore_writesync: false,
  fakestore_syncthreads: 4,
  fakestore_fakeattr: true,   

  // --- ebofs ---
  ebofs: 0,
  ebofs_commit_ms:      10000,      // 0 = no forced commit timeout (for debugging/tracing)
  ebofs_idle_commit_ms: 100,        // 0 = no idle detection.  use this -or- bdev_idle_kick_after_ms
  ebofs_oc_size:        10000,      // onode cache
  ebofs_cc_size:        10000,      // cnode cache
  ebofs_bc_size:        (150 *256), // 4k blocks, *256 for MB
  ebofs_bc_max_dirty:   (100 *256), // before write() will block
  
  ebofs_abp_zero: false,          // zero newly allocated buffers (may shut up valgrind)
  ebofs_abp_max_alloc: 4096*16,   // max size of new buffers (larger -> more memory fragmentation)

  uofs: 0,
  uofs_cache_size: 1<<18,       // 256MB
  uofs_onode_size:             (int)1024,
  uofs_small_block_size:       (int)4096,      //4KB
  uofs_large_block_size:       (int)524288,    //512KB
  uofs_segment_size:           (int)268435456, //256MB
  uofs_block_meta_ratio:       (int)10,
  uofs_sync_write:             (int)0,
  uofs_nr_hash_buckets:        (int)1023,
  uofs_flush_interval:         (int)5,         //seconds
  uofs_min_flush_pages:        (int)1024,      //4096 4k-pages
  uofs_delay_allocation:       (int)1,         //true

  // --- block device ---
  bdev_iothreads:    1,         // number of ios to queue with kernel
  bdev_idle_kick_after_ms: 0,//100, // ms   ** FIXME ** this seems to break things, not sure why yet **
  bdev_el_fw_max_ms: 1000,      // restart elevator at least once every 1000 ms
  bdev_el_bw_max_ms: 300,       // restart elevator at least once every 300 ms
  bdev_el_bidir: true,          // bidirectional elevator?
  bdev_iov_max: 512,            // max # iov's to collect into a single readv()/writev() call
  bdev_debug_check_io_overlap: true,   // [DEBUG] check for any pending io overlaps

  // --- fakeclient (mds regression testing) (ancient history) ---
  num_fakeclient: 100,
  fakeclient_requests: 100,
  fakeclient_deterministic: false,

  fakeclient_op_statfs:     false,

  // loosely based on Roselli workload paper numbers
  fakeclient_op_stat:     610,
  fakeclient_op_lstat:      false,
  fakeclient_op_utime:    0,
  fakeclient_op_chmod:    1,
  fakeclient_op_chown:    1,

  fakeclient_op_readdir:  2,
  fakeclient_op_mknod:    30,
  fakeclient_op_link:     false,
  fakeclient_op_unlink:   20,
  fakeclient_op_rename:   0,//40,

  fakeclient_op_mkdir:    10,
  fakeclient_op_rmdir:    20,
  fakeclient_op_symlink:  20,

  fakeclient_op_openrd:   200,
  fakeclient_op_openwr:   0,
  fakeclient_op_openwrc:  0,
  fakeclient_op_read:       false,  // osd!
  fakeclient_op_write:      false,  // osd!
  fakeclient_op_truncate:   false,
  fakeclient_op_fsync:      false,
  fakeclient_op_close:    200
};


#include <stdlib.h>
#include <string.h>
#include <iostream>
using namespace std;


void argv_to_vec(int argc, char **argv,
				 vector<char*>& args)
{
  for (int i=1; i<argc; i++)
	args.push_back(argv[i]);
}

void vec_to_argv(vector<char*>& args,
				 int& argc, char **&argv)
{
  argv = (char**)malloc(sizeof(char*) * argc);
  argc = 1;
  argv[0] = "asdf";

  for (unsigned i=0; i<args.size(); i++) 
	argv[argc++] = args[i];
}

void parse_config_options(vector<char*>& args)
{
  vector<char*> nargs;

  for (unsigned i=0; i<args.size(); i++) {
	if (strcmp(args[i], "--nummds") == 0) 
	  g_conf.num_mds = atoi(args[++i]);
	else if (strcmp(args[i], "--numclient") == 0) 
	  g_conf.num_client = atoi(args[++i]);
	else if (strcmp(args[i], "--numosd") == 0) 
	  g_conf.num_osd = atoi(args[++i]);

	else if (strcmp(args[i], "--tcp_skip_rank0") == 0)
	  g_conf.tcp_skip_rank0 = true;

	else if (strcmp(args[i], "--mkfs") == 0) 
	  g_conf.osd_mkfs = g_conf.mkfs = 1; //atoi(args[++i]);

	else if (strcmp(args[i], "--fake_osdmap_expand") == 0) 
	  g_conf.fake_osdmap_expand = atoi(args[++i]);
	//else if (strcmp(args[i], "--fake_osd_sync") == 0) 
	//g_conf.fake_osd_sync = atoi(args[++i]);

	else if (strcmp(args[i], "--debug") == 0) 
	  g_conf.debug = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_mds_balancer") == 0) 
	  g_conf.debug_mds_balancer = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_mds_log") == 0) 
	  g_conf.debug_mds_log = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_buffer") == 0) 
	  g_conf.debug_buffer = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_filer") == 0) 
	  g_conf.debug_filer = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_client") == 0) 
	  g_conf.debug_client = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_osd") == 0) 
	  g_conf.debug_osd = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_ebofs") == 0) 
	  g_conf.debug_ebofs = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_bdev") == 0) 
	  g_conf.debug_bdev = atoi(args[++i]);
	else if (strcmp(args[i], "--debug_ns") == 0) 
	  g_conf.debug_ns = atoi(args[++i]);

	else if (strcmp(args[i], "--log") == 0) 
	  g_conf.log = atoi(args[++i]);
	else if (strcmp(args[i], "--log_name") == 0) 
	  g_conf.log_name = args[++i];

	else if (strcmp(args[i], "--fakemessenger_serialize") == 0) 
	  g_conf.fakemessenger_serialize = atoi(args[++i]);

	else if (strcmp(args[i], "--mds_cache_size") == 0) 
	  g_conf.mds_cache_size = atoi(args[++i]);

	else if (strcmp(args[i], "--mds_log") == 0) 
	  g_conf.mds_log = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_log_before_reply") == 0) 
	  g_conf.mds_log_before_reply = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_log_max_len") == 0) 
	  g_conf.mds_log_max_len = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_log_read_inc") == 0) 
	  g_conf.mds_log_read_inc = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_log_max_trimming") == 0) 
	  g_conf.mds_log_max_trimming = atoi(args[++i]);

	else if (strcmp(args[i], "--mds_commit_on_shutdown") == 0) 
	  g_conf.mds_commit_on_shutdown = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_log_flush_on_shutdown") == 0) 
	  g_conf.mds_log_flush_on_shutdown = atoi(args[++i]);

	else if (strcmp(args[i], "--mds_bal_interval") == 0) 
	  g_conf.mds_bal_interval = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_bal_rep") == 0) 
	  g_conf.mds_bal_replicate_threshold = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_bal_unrep") == 0) 
	  g_conf.mds_bal_unreplicate_threshold = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_bal_max") == 0) 
	  g_conf.mds_bal_max = atoi(args[++i]);
	else if (strcmp(args[i], "--mds_bal_max_until") == 0) 
	  g_conf.mds_bal_max_until = atoi(args[++i]);

	else if (strcmp(args[i], "--client_cache_size") == 0)
	  g_conf.client_cache_size = atoi(args[++i]);
	else if (strcmp(args[i], "--client_cache_stat_ttl") == 0)
	  g_conf.client_cache_stat_ttl = atoi(args[++i]);
	else if (strcmp(args[i], "--client_trace") == 0)
	  g_conf.client_trace = atoi(args[++i]);
	else if (strcmp(args[i], "--fuse_direct_io") == 0)
	  g_conf.fuse_direct_io = atoi(args[++i]);

	else if (strcmp(args[i], "--client_sync_writes") == 0)
	  g_conf.client_sync_writes = atoi(args[++i]);
	else if (strcmp(args[i], "--client_bcache") == 0)
	  g_conf.client_bcache = atoi(args[++i]);
	else if (strcmp(args[i], "--client_bcache_ttl") == 0)
	  g_conf.client_bcache_ttl = atoi(args[++i]);


	else if (strcmp(args[i], "--ebofs") == 0) 
	  g_conf.ebofs = 1;
	else if (strcmp(args[i], "--ebofs_commit_ms") == 0)
	  g_conf.ebofs_commit_ms = atoi(args[++i]);


	else if (strcmp(args[i], "--fakestore") == 0) {
	  g_conf.ebofs = 0;
	  g_conf.osd_pg_bits = 5;
	  //g_conf.fake_osd_sync = 2;
	}
	else if (strcmp(args[i], "--fakestore_fsync") == 0) 
	  g_conf.fakestore_fsync = atoi(args[++i]);
	else if (strcmp(args[i], "--fakestore_writesync") == 0) 
	  g_conf.fakestore_writesync = atoi(args[++i]);

	else if (strcmp(args[i], "--obfs") == 0) {
	  g_conf.uofs = 1;
	  //g_conf.fake_osd_sync = 2;
	}

	else if (strcmp(args[i], "--osd_mkfs") == 0) 
	  g_conf.osd_mkfs = atoi(args[++i]);
	else if (strcmp(args[i], "--osd_pg_bits") == 0) 
	  g_conf.osd_pg_bits = atoi(args[++i]);
	else if (strcmp(args[i], "--osd_max_rep") == 0) 
	  g_conf.osd_max_rep = atoi(args[++i]);
	else if (strcmp(args[i], "--osd_maxthreads") == 0) 
	  g_conf.osd_maxthreads = atoi(args[++i]);

	else if (strcmp(args[i], "--bdev_iothreads") == 0) 
	  g_conf.bdev_iothreads = atoi(args[++i]);
	else if (strcmp(args[i], "--bdev_idle_kick_after_ms") == 0) 
	  g_conf.bdev_idle_kick_after_ms = atoi(args[++i]);


	else {
	  nargs.push_back(args[i]);
	}
  }

  args = nargs;
}
