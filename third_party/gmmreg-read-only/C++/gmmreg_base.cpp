/*=========================================================================
$Author: bingjian $
$Date: 2011-09-06 01:51:56 -0400 (Tue, 06 Sep 2011) $
$Revision: 141 $
=========================================================================*/

/**
 * \file gmmreg_base.cpp
 * \brief  The definition of the base class
 */

/*#ifdef WIN32
#include <windows.h>
#else
#include "port_ini.h"
#endif
*/

#include <assert.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

#include <vcl_string.h>
#include <vcl_iostream.h>

#include "gmmreg_base.h"
#include "gmmreg_utils.h"


void gmmreg_base::run(const char* f_config) {
  initialize(f_config);
  vnl_vector<double> params;
  start_registration(params);
  save_results(f_config, params);
}

int gmmreg_base::initialize(const char* f_config) {
  if (prepare_input(f_config) < 0) {
    return -1;
  }
  set_init_params(f_config);
  prepare_common_options(f_config);
  prepare_own_options(f_config);
  prepare_basis_kernel();
  return 0;
}

int gmmreg_base::prepare_input(const char* f_config) {
  char f_model[80]={0}, f_scene[80]={0};
  GetPrivateProfileString(common_section, "model", NULL,
      f_model, 80, f_config);
  if (set_model(f_model) < 0) {
    return -1;
  }
  GetPrivateProfileString(common_section, "scene", NULL,
      f_scene, 80, f_config);
  if (set_scene(f_scene) < 0) {
    return -1;
  }
  return 0;
}

// load model point set
int gmmreg_base::set_model(const char* filename) {
  std::ifstream infile(filename, std::ios_base::in);
  if (infile.is_open()) {
    if (model.read_ascii(infile)) {
      m = model.rows();
      d = model.cols();
      transformed_model.set_size(m, d);
      //std::cout << m << "," << d << std::endl;
      return m;
    } else {
      std::cerr << "unable to parse input file " << filename << " as a matrix." <<std::endl;
      return -1;
    }
  } else {
    std::cerr << "unable to open model file " << filename << std::endl;
    return -1;
  }
}

// load scene point set
int gmmreg_base::set_scene(const char* filename) {
  std::ifstream infile(filename, std::ios_base::in);
  if (infile.is_open()) {
    if(scene.read_ascii(infile)) {
      s = scene.rows();
      assert(scene.cols()==d);
      return s;
    } else {
      std::cerr << "unable to parse input file " << filename << " as a matrix." <<std::endl;
      return -1;
    }
  } else {
    std::cerr << "unable to open scene file " << filename << std::endl;
    return -1;
  }
}

int gmmreg_base::set_ctrl_pts(const char* filename) {
  if (strlen(filename) == 0) {
    std::cout << "The control point set is not specified, the model points are used as control points." << std::endl;
    ctrl_pts = model;
    n = ctrl_pts.rows();
    // std::cout << m << "," << n << std::endl;
    return n;
  } else {
    std::ifstream infile(filename, std::ios_base::in);
    if (infile.is_open()) {
      ctrl_pts.read_ascii(infile);
      assert(ctrl_pts.cols()==d);
      n = ctrl_pts.rows();
      return n;
    } else {
      std::cerr << "unable to open control points file " << filename << std::endl;
      return -1;
    }
  }
}

void gmmreg_base::save_transformed(const char* filename,
    const vnl_vector<double>& params, const char* f_config) {
  std::ofstream outfile(filename,std::ios_base::out);
  perform_transform(params);
  if (b_normalize) {
    denormalize(transformed_model,scene_centroid, scene_scale);
    denormalize(model,scene_centroid, scene_scale);
    denormalize(scene,scene_centroid, scene_scale);
  }
  transformed_model.print(outfile);

  char section_correspondence[80] = "CORRESPONDENCE";
  int num = GetPrivateProfileInt(section_correspondence, "num_of_thresholds", 0, f_config);
  if (num>0) {
    char s_min[256], s_max[256], s_pairs[256];
    GetPrivateProfileString(section_correspondence, "min_threshold", NULL, s_min, 255, f_config);
    GetPrivateProfileString(section_correspondence, "max_threshold", NULL, s_max, 255, f_config);
    GetPrivateProfileString(section_correspondence, "matched_pairs", NULL, s_pairs, 255, f_config);
    std::ofstream f_pair(s_pairs,std::ios_base::out);
    double min_threshold, max_threshold, interval;
    min_threshold = atof(s_min);
    max_threshold = atof(s_max);
    if (num==1)
      interval = 0.0f;
    else
      interval = (max_threshold - min_threshold)/(num-1);
    //vnl_matrix<double> working_M, working_S;
    vnl_matrix<double> dist;
    vnl_matrix<int> pairs;
    ComputeSquaredDistanceMatrix(transformed_model, scene, dist);
    for (int i=0;i<num;++i) {
      double threshold  = min_threshold + i*interval;
      //int n_match = find_working_pair(model, scene,transformed_model, threshold, working_M, working_S);
      pick_indices(dist, pairs, threshold*threshold);
      //printf("%f : %d\n",threshold, n_match);
      f_pair << "distance threshold : " << threshold << std::endl;
      f_pair << "# of matched point pairs : " << pairs.cols() << std::endl;
      int j;
      for (j=0; j<pairs.cols(); ++j) {
        f_pair.width(6);
        f_pair << std::left << pairs(0,j);
      }
      f_pair << std::endl;
      for (j=0; j<pairs.cols(); ++j) {
        f_pair.width(6);
        f_pair << std::left << pairs(1,j);
      }
      f_pair << std::endl;
    }
  }
  std::cout<<"Please find the transformed model set in "<<filename<<std::endl;
}

void gmmreg_base::prepare_common_options(const char* f_config) {
  b_normalize = GetPrivateProfileInt(section, "normalize", 1, f_config);
  if (b_normalize) {
    normalize(model,model_centroid,model_scale);
    normalize(scene,scene_centroid,scene_scale);
    normalize(ctrl_pts,model_centroid,model_scale);
  }
}

void gmmreg_base::multi_scale_options(const char* f_config) {
  level = GetPrivateProfileInt(section, "level", 1, f_config);
  char s_scale[256]={0}, s_func_evals[256] ={0};
  char delims[] = " -,;";
  GetPrivateProfileString(section, "sigma", NULL, s_scale, 255, f_config);
  parse_tokens(s_scale, delims,v_scale);
  if (v_scale.size()<level) {
    std::cerr<< " too many levels " << std::endl;
    exit(1);
  }
  GetPrivateProfileString(section, "max_function_evals", NULL, s_func_evals, 255, f_config);
  parse_tokens(s_func_evals, delims,v_func_evals);
  if (v_func_evals.size()<level) {
    std::cerr<< " too many levels " << std::endl;
    exit(1);
  }
}
