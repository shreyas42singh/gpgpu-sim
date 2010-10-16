/* 
 * Copyright © 2009 by Tor M. Aamodt, Wilson W. L. Fung and the University of 
 * British Columbia, Vancouver, BC V6T 1Z4, All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING GPGPU-SIM, YOU ARE AGREEING TO THESE
 * TERMS AND CONDITIONS.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * NOTE: The files libcuda/cuda_runtime_api.c and src/cuda-sim/cuda-math.h
 * are derived from the CUDA Toolset available from http://www.nvidia.com/cuda
 * (property of NVIDIA).  The files benchmarks/BlackScholes/ and 
 * benchmarks/template/ are derived from the CUDA SDK available from 
 * http://www.nvidia.com/cuda (also property of NVIDIA).  The files from 
 * src/intersim/ are derived from Booksim (a simulator provided with the 
 * textbook "Principles and Practices of Interconnection Networks" available 
 * from http://cva.stanford.edu/books/ppin/). As such, those files are bound by 
 * the corresponding legal terms and conditions set forth separately (original 
 * copyright notices are left in files from these sources and where we have 
 * modified a file our copyright notice appears before the original copyright 
 * notice).  
 * 
 * Using this version of GPGPU-Sim requires a complete installation of CUDA 
 * which is distributed seperately by NVIDIA under separate terms and 
 * conditions.  To use this version of GPGPU-Sim with OpenCL requires a
 * recent version of NVIDIA's drivers which support OpenCL.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the University of British Columbia nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 4. This version of GPGPU-SIM is distributed freely for non-commercial use only.  
 *  
 * 5. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 6. GPGPU-SIM was developed primarily by Tor M. Aamodt, Wilson W. L. Fung, 
 * Ali Bakhoda, George L. Yuan, at the University of British Columbia, 
 * Vancouver, BC V6T 1Z4
 */

#include "visualizer.h"

#include "gpu-sim.h"
#include "l2cache.h"
#include "shader.h"
#include "../option_parser.h"
#include "mem_latency_stat.h"
#include "stat-tool.h"
#include "gpu-cache.h"

#include <time.h>
#include <string.h>
#include <zlib.h>

static void time_vector_print_interval2gzfile(gzFile outfile);

bool g_visualizer_enabled;
char *g_visualizer_filename;
int g_visualizer_zlevel;

void visualizer_options(option_parser_t opp)
{
   option_parser_register(opp, "-visualizer_enabled", OPT_BOOL,
                          &g_visualizer_enabled, "Turn on visualizer output (1=On, 0=Off)",
                          "1");

   option_parser_register(opp, "-visualizer_outputfile", OPT_CSTR, 
                          &g_visualizer_filename, "Specifies the output log file for visualizer",
                          NULL);

   option_parser_register(opp, "-visualizer_zlevel", OPT_INT32,
                          &g_visualizer_zlevel, "Compression level of the visualizer output log (0=no comp, 9=highest)",
                          "6");

}

void gpgpu_sim::visualizer_printstat()
{
   gzFile visualizer_file = NULL; // gzFile is basically a pointer to a struct, so it is fine to initialize it as NULL
   if ( !g_visualizer_enabled )
      return;
   // initialize file name if it is not set 
   if ( g_visualizer_filename == NULL ) {
      time_t curr_time;
      time(&curr_time);
      char *date = ctime(&curr_time);
      char *s = date;
      while (*s) {
         if (*s == ' ' || *s == '\t' || *s == ':') *s = '-';
         if (*s == '\n' || *s == '\r' ) *s = 0;
         s++;
      }
      char buf[1024];
      snprintf(buf,1024,"gpgpusim_visualizer__%s.log.gz",date);
      g_visualizer_filename = strdup(buf);
   }

   // clean the content of the visualizer log if it is the first time, otherwise attach at the end
   static bool visualizer_first_printstat = true;
   visualizer_file = gzopen(g_visualizer_filename, (visualizer_first_printstat)? "w" : "a");
   if (visualizer_file == NULL) {
      printf("error - could not open visualizer trace file.\n");
      exit(1);
   }
   gzsetparams(visualizer_file, g_visualizer_zlevel, Z_DEFAULT_STRATEGY);
   visualizer_first_printstat = false;

   /*
   // instruction count per shader core
   gzprintf(visualizer_file, "shaderinsncount:  ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%u ",m_sc[i]->get_num_sim_insn());
   gzprintf(visualizer_file, "\n");

   // warp divergence per shader core
   gzprintf(visualizer_file, "shaderwarpdiv: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%u ", m_sc[i]->get_n_diverge());
   gzprintf(visualizer_file, "\n");
    */

   cflog_visualizer_gzprint(visualizer_file);
   shader_CTA_count_visualizer_gzprint(visualizer_file);

   // per shader core cache miss rate 
/*
   gzprintf(visualizer_file, "CacheMissRate_GlobalLocalL1_All: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1_windowed_cache_miss_rate(0));
   gzprintf(visualizer_file, "\n");
   gzprintf(visualizer_file, "CacheMissRate_TextureL1_All: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1tex_windowed_cache_miss_rate(0));
   gzprintf(visualizer_file, "\n");
   gzprintf(visualizer_file, "CacheMissRate_ConstL1_All: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1const_windowed_cache_miss_rate(0));
   gzprintf(visualizer_file, "\n");
   gzprintf(visualizer_file, "CacheMissRate_GlobalLocalL1_noMgHt: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1_windowed_cache_miss_rate(1));
   gzprintf(visualizer_file, "\n");
   gzprintf(visualizer_file, "CacheMissRate_TextureL1_noMgHt: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1tex_windowed_cache_miss_rate(1));
   gzprintf(visualizer_file, "\n");
   gzprintf(visualizer_file, "CacheMissRate_ConstL1_noMgHt: ");
   for (unsigned i=0;i<m_n_shader;i++) 
      gzprintf(visualizer_file, "%0.4f ", m_sc[i]->L1const_windowed_cache_miss_rate(1));
   gzprintf(visualizer_file, "\n");
   // reset for next interval
   for (unsigned i=0;i<m_n_shader;i++) 
      m_sc[i]->new_cache_window();
*/

   for (unsigned i=0;i<m_memory_config->m_n_mem;i++) 
      m_memory_partition_unit[i]->visualizer_print(visualizer_file);

   // overall cache miss rates
   gzprintf(visualizer_file, "Lonetexturemiss: %d\n", m_shader_stats->L1_texture_miss);
   gzprintf(visualizer_file, "Loneconstmiss: %d\n", m_shader_stats->L1_const_miss);
   gzprintf(visualizer_file, "Lonereadmiss: %d\n", m_shader_stats->L1_read_miss);
   gzprintf(visualizer_file, "Lonewritemiss: %d\n", m_shader_stats->L1_write_miss);
   gzprintf(visualizer_file, "Ltwowritemiss: %d\n", m_memory_stats->L2_write_miss);
   gzprintf(visualizer_file, "Ltwowritehit: %d\n",  m_memory_stats->L2_write_hit);
   gzprintf(visualizer_file, "Ltworeadmiss: %d\n", m_memory_stats->L2_read_miss);
   gzprintf(visualizer_file, "Ltworeadhit: %d\n", m_memory_stats->L2_read_hit);

   // latency stats
   if (m_memory_stats->num_mfs) {
      gzprintf(visualizer_file, "averagemflatency: %lld\n", m_memory_stats->mf_total_lat/m_memory_stats->num_mfs);
   }

   // other parameters for graphing
   gzprintf(visualizer_file, "globalcyclecount: %lld\n", gpu_sim_cycle);
   gzprintf(visualizer_file, "globalinsncount: %lld\n", gpu_sim_insn);
   gzprintf(visualizer_file, "globaltotinsncount: %lld\n", gpu_tot_sim_insn);
   gzprintf(visualizer_file, "gpucompletedthreads: %lld\n", m_shader_stats->gpu_completed_thread);
   gzprintf(visualizer_file, "gpgpu_n_cache_bkconflict: %d\n", m_shader_stats->gpgpu_n_cache_bkconflict);
   gzprintf(visualizer_file, "gpgpu_n_shmem_bkconflict: %d\n", m_shader_stats->gpgpu_n_shmem_bkconflict);     
   gzprintf(visualizer_file, "gpu_stall_by_MSHRwb: %d\n", m_shader_stats->gpu_stall_by_MSHRwb);   

   // warp divergence breakdown
   static unsigned int *last_shader_cycle_distro = NULL;
   if (!last_shader_cycle_distro)
      last_shader_cycle_distro = (unsigned int*) calloc(m_shader_config->warp_size + 3, sizeof(unsigned int));
   time_vector_print_interval2gzfile(visualizer_file);
   gzprintf(visualizer_file, "WarpDivergenceBreakdown:");
   unsigned int total=0;
   unsigned int cf = (m_shader_config->gpgpu_warpdistro_shader==-1)?num_shader():1;
   gzprintf(visualizer_file, " %d", (m_shader_stats->shader_cycle_distro[0] - last_shader_cycle_distro[0]) / cf );
   gzprintf(visualizer_file, " %d", (m_shader_stats->shader_cycle_distro[2] - last_shader_cycle_distro[2]) / cf );
   for (unsigned i=0; i<m_shader_config->warp_size+3; i++) {
      if ( i>=3 ) {
         total += (m_shader_stats->shader_cycle_distro[i] - last_shader_cycle_distro[i]);
         if ( ((i-3) % (m_shader_config->warp_size/8)) == ((m_shader_config->warp_size/8)-1) ) {
            gzprintf(visualizer_file, " %d", total / cf );
            total=0;
         }
      }
      last_shader_cycle_distro[i] = m_shader_stats->shader_cycle_distro[i];
   }
   gzprintf(visualizer_file,"\n");

   gzclose(visualizer_file);
}

#include <list>
#include <vector>
#include <iostream>
#include <map>
#include"../gpgpu-sim/shader.h"
class my_time_vector {
private:
   std::map< unsigned int, std::vector<long int> > ld_time_map;
   std::map< unsigned int, std::vector<long int> > st_time_map;
   unsigned ld_vector_size;
   unsigned st_vector_size;
   std::vector<double>  ld_time_dist;
   std::vector<double>  st_time_dist;

   std::vector<double>  overal_ld_time_dist;
   std::vector<double>  overal_st_time_dist;
   int overal_ld_count;
   int overal_st_count;

public:
   my_time_vector(int ld_size,int st_size){
      ld_vector_size = ld_size;
      st_vector_size = st_size;
      ld_time_dist.resize(ld_size);
      st_time_dist.resize(st_size);
      overal_ld_time_dist.resize(ld_size);
      overal_st_time_dist.resize(st_size);
      overal_ld_count = 0;
      overal_st_count= 0;
   }
   void update_ld(unsigned int uid,unsigned int slot, long int time) { 
      if ( ld_time_map.find( uid )!=ld_time_map.end() ) {
         ld_time_map[uid][slot]=time;
      } else if (slot <= MR_2SH_FQ_POP ) {
         std::vector<long int> time_vec;
         time_vec.resize(ld_vector_size);
         time_vec[slot] = time;
         ld_time_map[uid] = time_vec;  
      } else {
         //It's a merged mshr! forget it
      }
   }
   void update_st(unsigned int uid,unsigned int slot, long int time) { 
      if ( st_time_map.find( uid )!=st_time_map.end() ) {
         st_time_map[uid][slot]=time;
      } else {
         std::vector<long int> time_vec;
         time_vec.resize(st_vector_size);
         time_vec[slot] = time;
         st_time_map[uid] = time_vec;  
      } 
   }
   void check_ld_update(unsigned int uid,unsigned int slot, long int latency) { 
      if ( ld_time_map.find( uid )!=ld_time_map.end() ) {
         int our_latency = ld_time_map[uid][slot] - ld_time_map[uid][MR_ICNT_PUSHED];
         assert( our_latency == latency);
      } else if (slot <= MR_2SH_FQ_POP ) {
         abort();
      }
   }
   void check_st_update(unsigned int uid,unsigned int slot, long int latency) { 
      if ( st_time_map.find( uid )!=st_time_map.end() ) {
         int our_latency = st_time_map[uid][slot] - st_time_map[uid][MR_ICNT_PUSHED];
         assert( our_latency == latency);
      } else {
         abort();
      } 
   }
private:
   void calculate_ld_dist(void) {
      unsigned i,first;
      long int last_update,diff;
      int finished_count=0;
      ld_time_dist.clear();
      ld_time_dist.resize(ld_vector_size);
      std::map< unsigned int, std::vector<long int> >::iterator iter, iter_temp;
      iter =ld_time_map.begin() ;
      while (iter != ld_time_map.end()) {
         last_update=0;
         first=-1;
         if (!iter->second[MR_WRITEBACK]) {
            //this request is not done yet skip it!
            ++iter; 
            continue;
         }
         while ( !last_update ) {
            first++;
            assert( first < iter->second.size() );
            last_update = iter->second[first];
         }

         for ( i=first;i<ld_vector_size;i++ ) {
            diff = iter->second[i] - last_update;
            if ( diff>0 ) {
               ld_time_dist[i]+=diff;
               last_update = iter->second[i];               
            }
         }
         iter_temp = iter;
         iter++;
         ld_time_map.erase(iter_temp);
         finished_count++;
      }
      if ( finished_count ) {
         for ( i=0;i<ld_vector_size;i++ ) {
            overal_ld_time_dist[i] = (overal_ld_time_dist[i]*overal_ld_count + ld_time_dist[i]) /  (overal_ld_count + finished_count); 
         }
         overal_ld_count += finished_count;        
         for ( i=0;i<ld_vector_size;i++ ) {
            ld_time_dist[i]/=finished_count;
         }
      }
   }

   void calculate_st_dist(void) {
      unsigned i,first;
      long int last_update,diff;
      int finished_count=0;
      st_time_dist.clear();
      st_time_dist.resize(st_vector_size);
      std::map< unsigned int, std::vector<long int> >::iterator iter,iter_temp;
      iter =st_time_map.begin() ;
      while (  iter != st_time_map.end() ) {
         last_update=0;
         first=-1;
         if (!iter->second[MR_2SH_ICNT_PUSHED]) {
            //this request is not done yet skip it!
            ++iter;
            continue;
         }
         while ( !last_update ) {
            first++;
            assert( first < iter->second.size() );
            last_update = iter->second[first];
         }

         for ( i=first;i<st_vector_size;i++ ) {
            diff = iter->second[i] - last_update;
            if ( diff>0 ) {
               st_time_dist[i]+=diff;
               last_update = iter->second[i];               
            }
         }
         iter_temp = iter;
         iter++;
         st_time_map.erase(iter_temp);
         finished_count++;       
      }
      if ( finished_count ) {
         for ( i=0;i<st_vector_size;i++ ) {
            overal_st_time_dist[i] = (overal_st_time_dist[i]*overal_st_count + st_time_dist[i]) /  (overal_st_count + finished_count); 
         }
         overal_st_count += finished_count;        
         for ( i=0;i<st_vector_size;i++ ) {
            st_time_dist[i]/=finished_count;
         }
      }
   }

public:
   void clear_time_map_vectors(void) {   
      ld_time_map.clear();
      st_time_map.clear();
   }
   void print_all_ld(void) {
      unsigned i;
      std::map< unsigned int, std::vector<long int> >::iterator iter;
      for ( iter =ld_time_map.begin() ; iter != ld_time_map.end(); ++iter ) {
         std::cout<<"ld_uid"<<iter->first;
         for ( i=0;i<ld_vector_size;i++ ) {
            std::cout<<" "<<iter->second[i];
         }
         std::cout<< std::endl;
      }
   }

   void print_all_st(void) {
      unsigned i;
      std::map< unsigned int, std::vector<long int> >::iterator iter;

      for ( iter =st_time_map.begin() ; iter != st_time_map.end(); ++iter ) {
         std::cout<<"st_uid"<<iter->first;
         for ( i=0;i<st_vector_size;i++ ) {
            std::cout<<" "<<iter->second[i];
         }
         std::cout<<std::endl;
      }
   }

   void calculate_dist() {
      calculate_ld_dist();
      calculate_st_dist();
   }
   void print_dist(void) {
      unsigned i; 
      calculate_dist();
      std::cout << "LD_mem_lat_dist " ;
      for ( i=0;i<ld_vector_size;i++ ) {
         std::cout <<" "<<(int)overal_ld_time_dist[i]; 
      }
      std::cout << std::endl;
      std::cout << "ST_mem_lat_dist " ;
      for ( i=0;i<st_vector_size;i++ ) {
         std::cout <<" "<<(int)overal_st_time_dist[i];
      }
      std::cout << std::endl;
   }
   void print_to_file(FILE *outfile) {
      unsigned i; 
      calculate_dist();
      fprintf (outfile,"LDmemlatdist:") ;
      for ( i=0;i<ld_vector_size;i++ ) {
         fprintf (outfile," %d", (int)ld_time_dist[i]); 
      }
      fprintf (outfile,"\n") ;
      fprintf (outfile,"STmemlatdist:") ;
      for ( i=0;i<st_vector_size;i++ ) {
         fprintf (outfile," %d", (int)st_time_dist[i]); 
      }
      fprintf (outfile,"\n") ;
   }   
   void print_to_gzfile(gzFile outfile) {
      unsigned i; 
      calculate_dist();
      gzprintf (outfile,"LDmemlatdist:") ;
      for ( i=0;i<ld_vector_size;i++ ) {
         gzprintf (outfile," %d", (int)ld_time_dist[i]); 
      }
      gzprintf (outfile,"\n") ;
      gzprintf (outfile,"STmemlatdist:") ;
      for ( i=0;i<st_vector_size;i++ ) {
         gzprintf (outfile," %d", (int)st_time_dist[i]); 
      }
      gzprintf (outfile,"\n") ;
   }   
};

my_time_vector* g_my_time_vector; 

void time_vector_create(int ld_size,int st_size) {
   g_my_time_vector = new my_time_vector(ld_size,st_size); 
}                               


void time_vector_print(void) {
   g_my_time_vector->print_dist();
}

void time_vector_print_interval2gzfile(gzFile outfile) {
   g_my_time_vector->print_to_gzfile(outfile);
}

#include "../gpgpu-sim/mem_fetch.h"

void time_vector_update(unsigned int uid,int slot ,long int cycle,int type) {
   if ( (type == RD_REQ) || (type == REPLY_DATA) ) {
      g_my_time_vector->update_ld( uid, slot,cycle);
   } else if ( type == WT_REQ ) {
      g_my_time_vector->update_st( uid, slot,cycle);
   } else {
      abort(); 
   }
}

void check_time_vector_update(unsigned int uid,int slot ,long int latency,int type) 
{
   if ( (type == RD_REQ) || (type == REPLY_DATA) ) {
      g_my_time_vector->check_ld_update( uid, slot, latency );
   } else if ( type == WT_REQ ) {
      g_my_time_vector->check_st_update( uid, slot, latency );
   } else {
      abort(); 
   }
}
