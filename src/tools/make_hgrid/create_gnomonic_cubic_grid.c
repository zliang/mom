#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mpp.h"
#include "constant.h"
#include "mosaic_util.h"
#include "tool_util.h"
#include "create_hgrid.h"
#define  D2R (M_PI/180.)
#define  R2D (180./M_PI)
#define  EPSLN10 (1.e-10)
#define  EPSLN4 (1.e-4)
#define  EPSLN5 (1.e-5)
#define  EPSLN7 (1.e-7)
#define  EPSLN8  (1.0e-8)

/* private subroutines */
void gnomonic_ed  (int ni, double* lamda, double* theta);
void gnomonic_angl(int ni, double* lamda, double* theta);
void gnomonic_dist(int ni, double* lamda, double* theta);
void cartesian_to_spherical(double x, double y, double z, double *lon, double *lat, double *r);
void spherical_to_cartesian(double lon, double lat, double r, double *x, double *y, double *z);
void symm_ed(int ni, double *lamda, double *theta);
void mirror_grid(int ni, int ntiles, double *x, double *y );
void mirror_latlon(double lon1, double lat1, double lon2, double lat2, double lon0,
		   double lat0, double *lon, double *lat);
void rot_3d(int axis, double x1in, double y1in, double z1in, double angle, double *x2out,
	    double *y2out, double *z2out, int degrees, int convert);
double excess_of_quad2(const double *vec1, const double *vec2, const double *vec3, const double *vec4 );
double angle_between_vectors2(const double *vec1, const double *vec2);
void plane_normal2(const double *P1, const double *P2, double *plane);
void calc_rotation_angle2(int nxp, double *x, double *y, double *angle_dx, double *angle_dy);
void cell_center(int ni, int nj, const double *lonc, const double *latc, double *lont, double *latt);
void cell_east(int ni, int nj, const double *lonc, const double *latc, double *lone, double *late);
void cell_north(int ni, int nj, const double *lonc, const double *latc, double *lonn, double *latn);
void calc_cell_area(int nx, int ny, const double *x, const double *y, double *area);
void direct_transform(double stretch_factor, int i1, int i2, int j1, int j2, double lon_p, double lat_p,
		      int n, double *lon, double *lat);
void setup_aligned_nest(int parent_tile, int parent_ni, int parent_nj,
			const double *parent_xc, const double *parent_yc, int halo,
			int refine_ratio, int istart, int iend, int jstart, int jend,
			double *x, double *y, double *dx, double *dy,
			double *area, double *angle_dx, double *angle_dy);
void spherical_linear_interpolation(double beta, const double *p1, const double *p2, double *pb);

/*******************************************************************************
  void create_gnomonic_cubic_grid( int *npoints, int *nratio, char *method, char *orientation, double *x,
                          double *y, double *dx, double *dy, double *area, double *angle_dx,
                          double *angle_dy )
  create nomomic cubic grid. All six tiles grid will be generated.
*******************************************************************************/
void create_gnomonic_cubic_grid( char* grid_type, int *nlon, int *nlat, double *x, double *y,
				double *dx, double *dy, double *area, double *angle_dx,
				 double *angle_dy, double shift_fac, int do_schmidt, double stretch_factor,
				 double target_lon, double target_lat, int nest_grid,
				 int parent_tile, int refine_ratio, int istart_nest,
				 int iend_nest, int jstart_nest, int jend_nest, int halo)
{
  const int ntiles = 6;
  int nx, ny, nxp, ni, nip;
  int i, j, n;
  double p1[2], p2[2];
  double *lon, *lat;
  double *xc, *yc, *xt, *yt;
  double *xe, *ye, *xn, *yn;
  int    stretched_grid=0;

  /* make sure the first 6 tiles have the same grid size and 
     the size in x and y-direction are the same
  */

  for(n=0; n<ntiles; n++) {
    if(nlon[n] != nlat[n] ) mpp_error("create_gnomonic_cubic_grid: the grid size in x and y-direction "
			  	  "should be the same for the 6 tiles of cubic sphere grid");
    if( nlon[n]%2 ) mpp_error("create_gnomonic_cubic_grid: supergrid size in x-direction should be divided by 2");
    if( nlat[n]%2 ) mpp_error("create_gnomonic_cubic_grid: supergrid size in y-direction should be divided by 2");
  }
  for(n=1; n<ntiles; n++) {
    if(nlon[n] != nlon[0]) mpp_error("create_gnomonic_cubic_grid: all six tiles should have same size");
  }
  
  nx  = nlon[0];
  ny  = nlat[0];
  ni  = nx/2;
  nip = ni + 1;
  nxp = nx+1;
  
  if ( do_schmidt && fabs(stretch_factor-1.) > EPSLN5 ) stretched_grid = 1;
  
  lon = (double *)malloc(nip*nip*sizeof(double));
  lat = (double *)malloc(nip*nip*sizeof(double));
  
  if(strcmp(grid_type, "gnomonic_ed")==0 )
    gnomonic_ed(  ni, lon, lat);
  else if(strcmp(grid_type,"gnomonic_dist")==0)
    gnomonic_dist(ni, lon, lat);
  else if(strcmp(grid_type,"gnomonic_angl")==0)
    gnomonic_angl(ni, lon, lat);
  else mpp_error("create_gnomonic_cubic_grid: grid type should be 'gnomonic_ed', "
		 "'gnomonic_dist' or 'gnomonic_angl'");

  symm_ed(ni, lon, lat);

  xc = (double *)malloc(ntiles*nip*nip*sizeof(double));
  yc = (double *)malloc(ntiles*nip*nip*sizeof(double));
  xt = (double *)malloc(ntiles*ni *ni *sizeof(double));
  yt = (double *)malloc(ntiles*ni *ni *sizeof(double));
  xe = (double *)malloc(ntiles*nip*ni *sizeof(double));
  ye = (double *)malloc(ntiles*nip*ni *sizeof(double));
  xn = (double *)malloc(ntiles*ni *nip*sizeof(double));
  yn = (double *)malloc(ntiles*ni *nip*sizeof(double));
  
  for(j=0; j<nip; j++) {
    for(i=0; i<nip; i++) {
      xc[j*nip+i] = lon[j*nip+i] - M_PI;
      yc[j*nip+i] = lat[j*nip+i];
    }
  }
  
  /* mirror_grid assumes that the tile=1 is centered on equator
     and greenwich meridian Lon[-pi,pi]  */
  mirror_grid(ni, ntiles, xc, yc);

  for(n=0; n<ntiles*nip*nip; n++) {
    /* This will result in the corner close to east coast of china */
    if( do_schmidt == 0 && shift_fac > EPSLN4) xc[n] -= M_PI/18.;
    if(xc[n] < 0.) xc[n] += 2.*M_PI;
    if(fabs(xc[n]) < EPSLN10) xc[n] = 0;
    if(fabs(yc[n]) < EPSLN10) yc[n] = 0;
  }
      
  /* ensure consistency on the boundary between tiles */
  for(j=0; j<nip; j++) {
    xc[  nip*nip+j*nip] = xc[j*nip+ni];                 /* 1E -> 2W */
    yc[  nip*nip+j*nip] = yc[j*nip+ni];                 /* 1E -> 2W */
    xc[2*nip*nip+j*nip] = xc[ni*nip+ni-j];              /* 1N -> 3W */
    yc[2*nip*nip+j*nip] = yc[ni*nip+ni-j];              /* 1N -> 3W */      
  }
  for(i=0; i<nip; i++) {
    xc[4*nip*nip+ni*nip+i] = xc[(ni-i)*nip];            /* 1W -> 5N */
    yc[4*nip*nip+ni*nip+i] = yc[(ni-i)*nip];            /* 1W -> 2N */
    xc[5*nip*nip+ni*nip+i] = xc[i];                     /* 1S -> 6N */
    yc[5*nip*nip+ni*nip+i] = yc[i];                     /* 1S -> 6N */      
    xc[2*nip*nip+i]        = xc[nip*nip+ni*nip+i];      /* 2N -> 3S */
    yc[2*nip*nip+i]        = yc[nip*nip+ni*nip+i];      /* 2N -> 3S */
    xc[3*nip*nip+i]        = xc[nip*nip+(ni-i)*nip+ni];  /* 2E -> 4S */
    yc[3*nip*nip+i]        = yc[nip*nip+(ni-i)*nip+ni];  /* 2E -> 4S */      
  }
  for(j=0; j<nip; j++) {
    xc[5*nip*nip+j*nip+ni] = xc[nip*nip+ni-j];          /* 2S -> 6E */
    yc[5*nip*nip+j*nip+ni] = yc[nip*nip+ni-j];          /* 2S -> 6E */  
    xc[3*nip*nip+j*nip]    = xc[2*nip*nip+j*nip+ni];    /* 3E -> 4W */
    yc[3*nip*nip+j*nip]    = yc[2*nip*nip+j*nip+ni];    /* 3E -> 4W */
    xc[4*nip*nip+j*nip]    = xc[2*nip*nip+ni*nip+ni-j]; /* 3N -> 5W */
    yc[4*nip*nip+j*nip]    = yc[2*nip*nip+ni*nip+ni-j]; /* 3N -> 5W */
  }
  for(i=0; i<nip; i++) {
    xc[4*nip*nip+i] = xc[3*nip*nip+ni*nip+i];           /* 4N -> 5S */
    yc[4*nip*nip+i] = yc[3*nip*nip+ni*nip+i];           /* 4N -> 5S */
    xc[5*nip*nip+i] = xc[3*nip*nip+(ni-i)*nip+ni];      /* 4E -> 6S */
    yc[5*nip*nip+i] = yc[3*nip*nip+(ni-i)*nip+ni];      /* 4E -> 6S */
  }
  for(j=0; j<nip; j++) {
    xc[5*nip*nip+j*nip] = xc[4*nip*nip+j*nip+ni];    /* 5E -> 6W */
    yc[5*nip*nip+j*nip] = yc[4*nip*nip+j*nip+ni];    /* 5E -> 6W */  
  }

  /* Schmidt transformation */
  if ( do_schmidt ) {
    for(n=0; n<ntiles; n++) {
       direct_transform(stretch_factor, 0, ni, 0, ni, target_lon*D2R, target_lat*D2R,
			n, xc+n*nip*nip, yc+n*nip*nip);
    }
  }
  
  /* calculate grid box center location */
       
  for(n=0; n<ntiles; n++) {
    cell_center(ni, ni, xc+n*nip*nip, yc+n*nip*nip, xt+n*ni*ni, yt+n*ni*ni);
    cell_east(ni, ni, xc+n*nip*nip, yc+n*nip*nip, xe+n*nip*ni, ye+n*nip*ni);
    cell_north(ni, ni, xc+n*nip*nip, yc+n*nip*nip, xn+n*ni*nip, yn+n*ni*nip);
  }

  /* copy grid box vertices location into super grid */
  for(n=0; n<ntiles; n++) for(j=0; j<nip; j++) for(i=0; i<nip; i++) {
    x[n*nxp*nxp+j*2*nxp+i*2]=xc[n*nip*nip+j*nip+i];
    y[n*nxp*nxp+j*2*nxp+i*2]=yc[n*nip*nip+j*nip+i];
  }

  /* copy grid box center location to super grid */
  for(n=0; n<ntiles; n++) for(j=0; j<ni; j++) for(i=0; i<ni; i++) {
    x[n*nxp*nxp+(j*2+1)*nxp+i*2+1]=xt[n*ni*ni+j*ni+i];
    y[n*nxp*nxp+(j*2+1)*nxp+i*2+1]=yt[n*ni*ni+j*ni+i];
  }  

  /* copy grid box east location to super grid */
  for(n=0; n<ntiles; n++) for(j=0; j<ni; j++) for(i=0; i<nip; i++) {
    x[n*nxp*nxp+(j*2+1)*nxp+i*2]=xe[n*nip*ni+j*nip+i];
    y[n*nxp*nxp+(j*2+1)*nxp+i*2]=ye[n*nip*ni+j*nip+i];
  }  

  /* copy grid box north location to super grid */
  for(n=0; n<ntiles; n++) for(j=0; j<nip; j++) for(i=0; i<ni; i++) {
    x[n*nxp*nxp+j*2*nxp+i*2+1]=xn[n*ni*nip+j*ni+i];
    y[n*nxp*nxp+j*2*nxp+i*2+1]=yn[n*ni*nip+j*ni+i];
  }  
  

  /* calculate grid cell length */
  for(n=0; n<ntiles; n++) {
    for(j=0; j<nxp; j++) {
      for(i=0; i<nx; i++) {
	p1[0] = x[n*nxp*nxp+j*nxp+i];
	p1[1] = y[n*nxp*nxp+j*nxp+i];
	p2[0] = x[n*nxp*nxp+j*nxp+i+1];
	p2[1] = y[n*nxp*nxp+j*nxp+i+1];
	dx[n*nx*nxp+j*nx+i] = great_circle_distance(p1, p2);
      }
    }
  }

  if( stretched_grid ) {
    for(n=0; n<ntiles; n++) {
      for(j=0; j<nx; j++) {
	for(i=0; i<nxp; i++) {
	  p1[0] = x[n*nxp*nxp+j*nxp+i];
	  p1[1] = y[n*nxp*nxp+j*nxp+i];
	  p2[0] = x[n*nxp*nxp+(j+1)*nxp+i];;
	  p2[1] = y[n*nxp*nxp+(j+1)*nxp+i];
	  dy[n*nx*nxp+j*nx+i] = great_circle_distance(p1, p2);
	}
      }
    }
  }
  else {
    for(n=0; n<ntiles; n++) {
      for(j=0; j<nxp; j++) {
	for(i=0; i<nx; i++) dy[n*nx*nxp+i*nxp+j] = dx[n*nx*nxp+j*nx+i];
      }
    }
  }
  
  /* ensure consistency on the boundaries between tiles */
  for(j=0; j<nx; j++) {
    dy[j*nxp]             = dx[4*nx*nxp+nx*nx+nx-j-1]; /* 5N -> 1W */
    dy[j*nxp+nx]          = dy[nxp*nx+j*nxp];          /* 2W -> 1E */
    dy[nxp*nx+j*nxp+nx]   = dx[3*nx*nxp+(nx-j-1)];     /* 4S -> 2E */
    dy[2*nxp*nx+j*nxp]    = dx[nx*nx+nx-j-1];          /* 1N -> 3W */
    dy[2*nxp*nx+j*nxp+nx] = dy[3*nxp*nx+j*nxp];        /* 4W -> 3E */
    dy[3*nxp*nx+j*nxp+nx] = dx[5*nx*nxp+(nx-j-1)];     /* 4S -> 2E */
    dy[4*nxp*nx+j*nxp]    = dx[2*nx*nxp+nx*nx+nx-j-1]; /* 3N -> 5W */
    dy[4*nxp*nx+j*nxp+nx] = dy[5*nxp*nx+j*nxp];        /* 6W -> 5E */
    dy[5*nxp*nx+j*nxp+nx] = dx[nx*nxp+(nx-j-1)];       /* 2S -> 6E */    
  }

  calc_cell_area(nx, ny, x, y, area);

  for(j=0; j<nx; j++) {
    for(i=0; i<nx; i++) {
      double ar;
      /* all the face have the same area */
      ar = area[j*nx+i];
      area[nx*nx+j*nx+i] = ar;
      area[2*nx*nx+j*nx+i] = ar;
      area[3*nx*nx+j*nx+i] = ar;
      area[4*nx*nx+j*nx+i] = ar;
      area[5*nx*nx+j*nx+i] = ar;        
    }
  }

  /*calculate rotation angle, just some workaround, will modify this in the future. */
  calc_rotation_angle2(nxp, x, y, angle_dx, angle_dy );

  if( nest_grid ) setup_aligned_nest(parent_tile, ni, ni, xc+nip*nip*(parent_tile-1),
				     yc+nip*nip*(parent_tile-1), halo, refine_ratio,
				     istart_nest, iend_nest, jstart_nest, jend_nest,
				     x+ntiles*nxp*nxp, y+ntiles*nxp*nxp,
				     dx+ntiles*nx*nxp, dy+ntiles*nxp*nx,
				     area+ntiles*nx*nx, angle_dx+ntiles*nxp*nxp,
				     angle_dy+ntiles*nxp*nxp);
  
  /* convert grid location from radians to degree */
  for(i=0; i<nxp*nxp*ntiles; i++) {
    x[i] = x[i]*R2D;
    y[i] = y[i]*R2D;
  }

  free(xt);
  free(yt);
  free(xc);
  free(yc);  
  free(xe);
  free(ye);
  free(xn);
  free(yn);
  
  
}; /* void create_gnomonic_cubic_grid */

void calc_cell_area(int nx, int ny, const double *x, const double *y, double *area)
{
  int i,j, nxp;
  double p_ll[2], p_ul[2], p_lr[2], p_ur[2];

  nxp = nx+1;
  for(j=0; j<ny; j++) {
    for(i=0; i<nx; i++) {
      p_ll[0] = x[j*nxp+i];       p_ll[1] = y[j*nxp+i];
      p_ul[0] = x[(j+1)*nxp+i];   p_ul[1] = y[(j+1)*nxp+i];
      p_lr[0] = x[j*nxp+i+1];     p_lr[1] = y[j*nxp+i+1];
      p_ur[0] = x[(j+1)*nxp+i+1]; p_ur[1] = y[(j+1)*nxp+i+1];
      /* all the face have the same area */
      area[j*nx+i] = spherical_excess_area(p_ll, p_ul, p_lr, p_ur, RADIUS);
    }
  }

}



/*-------------------------------------------------------------------------
  void direct_transform(double c, int i1, int i2, int j1, int j2, double lon_p, double lat_p, int n,
		        double *lon, double *lat)

  This is a direct transformation of the standard (symmetrical) cubic grid
  to a locally enhanced high-res grid on the sphere; it is an application
  of the Schmidt transformation at the south pole followed by a 
  pole_shift_to_target (rotation) operation

  arguments:
    c            : Stretching factor
    lon_p, lat_p : center location of the target face, radian
    n            : grid face number
    i1,i2,j1,j2  : starting and ending index in i- and j-direction
    lon          : longitude. 0 <= lon <= 2*pi
    lat          : latitude. -pi/2 <= lat <= pi/2
  ------------------------------------------------------------------------*/

void direct_transform(double stretch_factor, int i1, int i2, int j1, int j2, double lon_p, double lat_p,
		      int n, double *lon, double *lat)
{
#ifdef NO_QUAD_PRECISION
  double lat_t, sin_p, cos_p, sin_lat, cos_lat, sin_o, p2, two_pi;
  double c2p1, c2m1;
#else
  long double lat_t, sin_p, cos_p, sin_lat, cos_lat, sin_o, p2, two_pi;
  long double c2p1, c2m1;
#endif
  int i, j, l, nxp;

  nxp = i2-i1+1;
  p2 = 0.5*M_PI;
  two_pi = 2.*M_PI;
  if(n==0) printf("create_gnomonic_cubic_grid: Schmidt transformation: stretching factor=%g, center=(%g,%g)\n",
		  stretch_factor, lon_p, lat_p);

  c2p1 = 1. + stretch_factor*stretch_factor;
  c2m1 = 1. - stretch_factor*stretch_factor;

  sin_p = sin(lat_p);
  cos_p = cos(lat_p);

  for(j=j1; j<=j2; j++) for(i=i1; i<=i2; i++) {
      l = j*nxp+i;
      if ( fabs(c2m1) > EPSLN7 ) {
	sin_lat = sin(lat[l]); 
	lat_t   = asin( (c2m1+c2p1*sin_lat)/(c2p1+c2m1*sin_lat) );
      }
      else {
	lat_t = lat[l];
      }
      sin_lat = sin(lat_t);
      cos_lat = cos(lat_t); 
      sin_o = -(sin_p*sin_lat + cos_p*cos_lat*cos(lon[l]));
      if ( (1.-fabs(sin_o)) < EPSLN7 ) {    /* poles */
	lon[l] = 0.;
	lat[l] = (sin_o < 0) ? -p2:p2;
      }
      else {
	lat[l] = asin( sin_o );
	lon[l] = lon_p + atan2(-cos_lat*sin(lon[l]), -sin_lat*cos_p+cos_lat*sin_p*cos(lon[l]));
	if ( lon[l] < 0. )
	  lon[l] +=two_pi;
	else if( lon[l] >= two_pi )
	  lon[l] -=two_pi;
      }
  }
  
}; /* direct_transform */



/*-----------------------------------------------------
      void gnomonic_ed
  Equal distance along the 4 edges of the cubed sphere
  -----------------------------------------------------
  Properties: 
  * defined by intersections of great circles
  * max(dx,dy; global) / min(dx,dy; global) = sqrt(2) = 1.4142
  * Max(aspect ratio) = 1.06089
  * the N-S coordinate curves are const longitude on the 4 faces with equator 
  For C2000: (dx_min, dx_max) = (3.921, 5.545)    in km unit
  ! Ranges:
  ! lamda = [0.75*pi, 1.25*pi]
  ! theta = [-alpha, alpha]
  --------------------------------------------------------*/
void gnomonic_ed(int ni, double* lamda, double* theta)
{

  int i, j, n, nip;
  double dely;
  double *pp, *x, *y, *z;
  double rsq3, alpha;


  nip = ni + 1;
  rsq3 = 1./sqrt(3.);
  alpha = asin( rsq3 );

  dely = 2.*alpha/ni;

  /* Define East-West edges: */
  for(j=0; j<nip; j++) {
    lamda[j*nip]    = 0.75*M_PI;               /* West edge */
    lamda[j*nip+ni] = 1.25*M_PI;               /* East edge */
    theta[j*nip]    = -alpha + dely*j;       /* West edge */
    theta[j*nip+ni] = theta[j*nip];          /* East edge */
  }

  /* Get North-South edges by symmetry: */

  for(i=1; i<ni; i++) {
      mirror_latlon(lamda[0], theta[0], lamda[ni*nip+ni], theta[ni*nip+ni],  
		    lamda[i*nip], theta[i*nip], &lamda[i], &theta[i] );
      lamda[ni*nip+i] = lamda[i];
      theta[ni*nip+i] = -theta[i];
  }

  x = (double *)malloc(nip*nip*sizeof(double));
  y = (double *)malloc(nip*nip*sizeof(double));
  z = (double *)malloc(nip*nip*sizeof(double));
  /* Set 4 corners: */
  latlon2xyz(1, &lamda[0], &theta[0], &x[0], &y[0], &z[0]);
  latlon2xyz(1, &lamda[ni], &theta[ni], &x[ni], &y[ni], &z[ni]);
  latlon2xyz(1, &lamda[ni*nip], &theta[ni*nip], &x[ni*nip], &y[ni*nip], &z[ni*nip]);
  latlon2xyz(1, &lamda[ni*nip+ni], &theta[ni*nip+ni], &x[ni*nip+ni], &y[ni*nip+ni], &z[ni*nip+ni]);

  /* Map edges on the sphere back to cube:
     Intersections at x=-rsq3   */

  for(j=1; j<ni; j++) {
    n = j*nip;
    latlon2xyz(1, &lamda[n], &theta[n], &x[n], &y[n], &z[n]);
    y[n] = -y[n]*rsq3/x[n];
    z[n] = -z[n]*rsq3/x[n];
  }

  for(i=1; i<ni; i++) {
    latlon2xyz(1, &lamda[i], &theta[i], &x[i], &y[i], &z[i]);
    y[i] = -y[i]*rsq3/x[i];
    z[i] = -z[i]*rsq3/x[i];
  }    

  for(j=0; j<nip; j++)
    for(i=0; i<nip; i++) x[j*nip+i] = -rsq3;

  for(j=1;j<nip; j++) {
    for(i=1; i<nip; i++) {
      y[j*nip+i] = y[i];
      z[j*nip+i] = z[j*nip];
    }
  }

  xyz2latlon(nip*nip, x, y, z, lamda, theta);

}; /* gnomonic_ed */

/*----------------------------------------------------------
  void gnomonic_angl()
  This is the commonly known equi-angular grid
**************************************************************/

void gnomonic_angl(int ni, double* lamda, double* theta)
{



}; /* gnomonic_angl */

/*----------------------------------------------------------
  void gnomonic_dist()
  This is the commonly known equi-distance grid
**************************************************************/

void gnomonic_dist(int ni, double* lamda, double* theta)
{



}; /* gnomonic_dist */

/*------------------------------------------------------------------
        void mirror_latlon
   Given the "mirror" as defined by (lon1, lat1), (lon2, lat2), and center 
   of the sphere, compute the mirror image of (lon0, lat0) as  (lon, lat)
   ---------------------------------------------------------------*/
   
void mirror_latlon(double lon1, double lat1, double lon2, double lat2, double lon0,
		   double lat0, double *lon, double *lat)
{
  double p0[3], p1[3], p2[3], pp[3], nb[3];
  double pdot;
  int k;

  latlon2xyz(1, &lon0, &lat0, &p0[0], &p0[1], &p0[2]);
  latlon2xyz(1, &lon1, &lat1, &p1[0], &p1[1], &p1[2]);
  latlon2xyz(1, &lon2, &lat2, &p2[0], &p2[1], &p2[2]);
  vect_cross(p1, p2, nb);
     
  pdot = sqrt(nb[0]*nb[0]+nb[1]*nb[1]+nb[2]*nb[2]);
  for(k=0; k<3; k++) nb[k] = nb[k]/pdot;

  pdot = p0[0]*nb[0] + p0[1]*nb[1] + p0[2]*nb[2];
  for(k=0; k<3; k++) pp[k] = p0[k] - 2*pdot*nb[k];
  xyz2latlon(1, &pp[0], &pp[1], &pp[2], lon, lat);
    
}; /* mirror_latlon */

/*-------------------------------------------------------------------------
  void symm_ed(int ni, double *lamda, double *theta)
  ! Make grid symmetrical to i=ni/2+1 and j=nj/2+1
  ------------------------------------------------------------------------*/
void symm_ed(int ni, double *lamda, double *theta)
{
  
  int nip, i, j, ip, jp;
  double avg;
  
  nip = ni+1;

  for(j=1; j<nip; j++)
    for(i=1; i<ni; i++) lamda[j*nip+i] = lamda[i];
  
  for(j=0; j<nip; j++) {
    for(i=0; i<ni/2; i++) {
      ip = ni - i;
      avg = 0.5*(lamda[j*nip+i]-lamda[j*nip+ip]);
      lamda[j*nip+i] = avg + M_PI;
      lamda[j*nip+ip] = M_PI - avg;
      avg = 0.5*(theta[j*nip+i]+theta[j*nip+ip]);
      theta[j*nip+i] = avg;
      theta[j*nip+ip] = avg;      
    }
  }

  /* Make grid symmetrical to j=im/2+1 */
  for(j = 0; j<ni/2; j++) {
    jp = ni - j;
    for(i=1; i<ni; i++) {
      avg = 0.5*(lamda[j*nip+i]+lamda[jp*nip+i]);
      lamda[j*nip+i] = avg;
      lamda[jp*nip+i] = avg;
      avg = 0.5*(theta[j*nip+i]-theta[jp*nip+i]);
      theta[j*nip+i] = avg;
      theta[jp*nip+i] = -avg;
    }
  }
}/* symm_ed */

/*------------------------------------------------------------------------------
  void mirror_grid( )
  Mirror Across the 0-longitude
  ----------------------------------------------------------------------------*/
void mirror_grid(int ni, int ntiles, double *x, double *y )
{
  int nip, i, j, ip, jp, nt;
  double x1, y1, z1, x2, y2, z2, ang;

  nip = ni+1;
  
  for(j=0; j<ceil(nip/2.); j++) {
    jp = ni - j;
    for(i=0; i<ceil(nip/2.); i++) {
      ip = ni - i;
      x1 = 0.25 * (fabs(x[j*nip+i]) + fabs(x[j*nip+ip]) + fabs(x[jp*nip+i]) + fabs(x[jp*nip+ip]) );
      x[j*nip+i]   = x1 * (x[j*nip+i]   >=0 ? 1:-1);
      x[j*nip+ip]  = x1 * (x[j*nip+ip]  >=0 ? 1:-1);
      x[jp*nip+i]  = x1 * (x[jp*nip+i]  >=0 ? 1:-1);
      x[jp*nip+ip] = x1 * (x[jp*nip+ip] >=0 ? 1:-1);      

      y1 = 0.25 * (fabs(y[j*nip+i]) + fabs(y[j*nip+ip]) + fabs(y[jp*nip+i]) + fabs(y[jp*nip+ip]) );
      y[j*nip+i]   = y1 * (y[j*nip+i]   >=0 ? 1:-1);
      y[j*nip+ip]  = y1 * (y[j*nip+ip]  >=0 ? 1:-1);
      y[jp*nip+i]  = y1 * (y[jp*nip+i]  >=0 ? 1:-1);
      y[jp*nip+ip] = y1 * (y[jp*nip+ip] >=0 ? 1:-1);      
      
      /* force dateline/greenwich-meridion consitency */
      if( nip%2 ) {
	if( i == (nip-1)/2 ) {
	  x[j*nip+i] = 0.0;
	  x[jp*nip+i] = 0.0;
	}
      }
    }
  }

  /* define the the other five tiles. */
  for(nt=1; nt<ntiles; nt++) {
    for(j=0; j<nip; j++) {
      for(i=0; i<nip; i++) {
	x1 = x[j*nip+i];
	y1 = y[j*nip+i];
	z1 = RADIUS;
	switch (nt) {
	case 1: /* tile 2 */
	  ang = -90.;
	  rot_3d( 3, x1, y1, z1, ang, &x2, &y2, &z2, 1, 1);  /* rotate about the z-axis */
	  break;
	case 2: /* tile 3 */
	  ang = -90.;
	  rot_3d( 3, x1, y1, z1, ang, &x2, &y2, &z2, 1, 1);  /* rotate about the z-axis */
	  ang = 90.;
	  rot_3d( 1, x2, y2, z2, ang, &x1, &y1, &z1, 1, 1); /* rotate about the z-axis */
	  x2=x1;
	  y2=y1;
	  z2=z1;

	  /* force North Pole and dateline/greenwich-meridion consitency */
	  if(nip%2) {
	    if( (i==(nip-1)/2) && (i==j) ) {
	      x2 = 0;
	      y2 = M_PI*0.5;
	    }

	    if( (j==(nip-1)/2) && (i<(nip-1)/2) ) x2 = 0;
	    if( (j==(nip-1)/2) && (i>(nip-1)/2) ) x2 = M_PI;
	  }
	  break;
	case 3: /* tile 4 */
	  ang = -180.;
	  rot_3d( 3, x1, y1, z1, ang, &x2, &y2, &z2, 1, 1); /* rotate about the z-axis */
	  ang = 90.;
	  rot_3d( 1, x2, y2, z2, ang, &x1, &y1, &z1, 1, 1); /* rotate about the z-axis */
	  x2=x1;
	  y2=y1;
	  z2=z1;

	  /* force dateline/greenwich-meridion consitency */
	  if( nip%2 ) {
	    if( j == (nip-1)/2 ) x2 = M_PI;
	  }
	  break;
	case 4: /* tile 5 */
	  ang = 90.;
	  rot_3d( 3, x1, y1, z1, ang, &x2, &y2, &z2, 1, 1); /* rotate about the z-axis */
	  ang = 90.;
	  rot_3d( 2, x2, y2, z2, ang, &x1, &y1, &z1, 1, 1); /* rotate about the z-axis */
	  x2=x1;
	  y2=y1;
	  z2=z1;
	  break;
	case 5: /* tile 6 */
	  ang = 90.;
	  rot_3d( 2, x1, y1, z1, ang, &x2, &y2, &z2, 1, 1); /* rotate about the z-axis */
	  ang = 0.;
	  rot_3d( 3, x2, y2, z2, ang, &x1, &y1, &z1, 1, 1); /* rotate about the z-axis */
	  x2=x1;
	  y2=y1;
	  z2=z1;
	  
	  /* force South Pole and dateline/greenwich-meridion consitency */
	  if(nip%2) {
	    if( (i==(nip-1)/2) && (i==j) ) {
	      x2 = 0;
	      y2 = -M_PI*0.5;
	    }

	    if( (i==(nip-1)/2) && (j>(nip-1)/2) ) x2 = 0;
	    if( (i==(nip-1)/2) && (j<(nip-1)/2) ) x2 = M_PI;
	  }
	  break;
	}
	x[nt*nip*nip+j*nip+i] = x2;
	y[nt*nip*nip+j*nip+i] = y2;
      }
    }
  }
}; /* mirror_grid */


/*-------------------------------------------------------------------------------
  void rot_3d()
  rotate points on a sphere in xyz coords (convert angle from
  degrees to radians if necessary)
  -----------------------------------------------------------------------------*/
void rot_3d(int axis, double x1in, double y1in, double z1in, double angle, double *x2out,
	    double *y2out, double *z2out, int degrees, int convert)
{

  double x1, y1, z1, x2, y2, z2, c, s;
  
  if(convert)
    spherical_to_cartesian(x1in, y1in, z1in, &x1, &y1, &z1);
  else {
    x1=x1in;
    y1=y1in;
    z1=z1in;
  }

  if(degrees) angle = angle*D2R;

  c = cos(angle);
  s = sin(angle);

  switch (axis) {
  case 1:
    x2 =  x1;
    y2 =  c*y1 + s*z1;
    z2 = -s*y1 + c*z1;
    break;
  case 2:
    x2 = c*x1 - s*z1;
    y2 = y1;
    z2 = s*x1 + c*z1;
    break;
  case 3:
    x2 =  c*x1 + s*y1;
    y2 = -s*x1 + c*y1;
    z2 = z1;
    break;
  default:
    mpp_error("Invalid axis: must be 1 for X, 2 for Y, 3 for Z.");
  }
  
  if(convert)
    cartesian_to_spherical(x2, y2, z2, x2out, y2out, z2out);
  else {
    *x2out=x2;;
    *y2out=y2;
    *z2out=z2;
    }
} /* rot_3d */

/*-------------------------------------------------------------
  void cartesian_to_spherical(x, y, z, lon, lat, r)
  may merge with xyz2latlon in the future
  ------------------------------------------------------------*/
void cartesian_to_spherical(double x, double y, double z, double *lon, double *lat, double *r)
{

  *r = sqrt(x*x + y*y + z*z);
  if ( (fabs(x) + fabs(y)) < EPSLN10 )       /* poles */
    *lon = 0.;
  else
    *lon = atan2(y,x);    /* range: [-pi,pi] */


  *lat = acos(z/(*r)) - M_PI/2.;
};/* cartesian_to_spherical */

/*-------------------------------------------------------------------------------
  void spherical_to_cartesian
  convert from spheircal coordinates to xyz coords
  may merge with latlon2xyz in the future
  -----------------------------------------------------------------------------*/
void spherical_to_cartesian(double lon, double lat, double r, double *x, double *y, double *z)
{
  *x = r * cos(lon) * cos(lat);
  *y = r * sin(lon) * cos(lat);

  *z = -r * sin(lat);
} /* spherical_to_cartesian */


/*****************************************************************
   double* excess_of_quad(int ni, int nj, double *vec1, double *vec2, 
                          double *vec3, double *vec4 )
*******************************************************************/
double excess_of_quad2(const double *vec1, const double *vec2, const double *vec3, const double *vec4 )
{
  double plane1[3], plane2[3], plane3[3], plane4[3];
  double angle12, angle23, angle34, angle41, excess;
  double ang12, ang23, ang34, ang41;
  
  plane_normal2(vec1, vec2, plane1);
  plane_normal2(vec2, vec3, plane2);
  plane_normal2(vec3, vec4, plane3);
  plane_normal2(vec4, vec1, plane4);
  angle12 = angle_between_vectors2(plane2,plane1);
  angle23 = angle_between_vectors2(plane3,plane2);
  angle34 = angle_between_vectors2(plane4,plane3);
  angle41 = angle_between_vectors2(plane1,plane4);
  ang12 = M_PI-angle12;
  ang23 = M_PI-angle23;
  ang34 = M_PI-angle34;
  ang41 = M_PI-angle41;
  excess = ang12+ang23+ang34+ang41-2*M_PI;
  /*  excess = 2*M_PI - angle12 - angle23 - angle34 - angle41; */

  return excess;

}; /* excess_of_quad */

/******************************************************************************* 
double angle_between_vectors(const double *vec1, const double *vec2)
*******************************************************************************/

double angle_between_vectors2(const double *vec1, const double *vec2) {
  int n;
  double vector_prod, nrm1, nrm2;
  double angle;
  
  vector_prod=vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
  nrm1=pow(vec1[0],2)+pow(vec1[1],2)+pow(vec1[2],2);
  nrm2=pow(vec2[0],2)+pow(vec2[1],2)+pow(vec2[2],2);
  if(nrm1*nrm2>0)
    angle = acos( vector_prod/sqrt(nrm1*nrm2) );
  else
    angle = 0;
  
  return angle;
}; /* angle_between_vectors */


/***********************************************************************
   double* plane_normal(int ni, int nj, double *P1, double *P2)
***********************************************************************/

void plane_normal2(const double *P1, const double *P2, double *plane)
{
  double mag;
  
  plane[0] = P1[1] * P2[2] - P1[2] * P2[1];
  plane[1] = P1[2] * P2[0] - P1[0] * P2[2];
  plane[2] = P1[0] * P2[1] - P1[1] * P2[0];
  mag=sqrt(pow(plane[0],2) + pow(plane[1],2) + pow(plane[2],2));
  if(mag>0) {
    plane[0]=plane[0]/mag;
    plane[1]=plane[1]/mag;
    plane[2]=plane[2]/mag;
  }
  
}; /* plane_normal */

/******************************************************************

  void calc_rotation_angle()

******************************************************************/

void calc_rotation_angle2(int nxp, double *x, double *y, double *angle_dx, double *angle_dy)
{
  int ip1, im1, jp1, jm1, tp1, tm1, i, j, n, ntiles, nx;
  double lon_scale;

  nx = nxp-1;
  ntiles = 6;
  for(n=0; n<ntiles; n++) {
    for(j=0; j<nxp; j++) {
      for(i=0; i<nxp; i++) {
	lon_scale = cos(y[n*nxp*nxp+j*nxp+i]*D2R);
	tp1 = n;
	tm1 = n;
	ip1 = i+1;
	im1 = i-1;
	jp1 = j;
	jm1 = j;

        if(ip1 >= nxp) {  /* find the neighbor tile. */
	  if(n % 2 == 0) { /* tile 1, 3, 5 */
	    tp1 = n+1;
	    ip1 = 0;
	  }
	  else { /* tile 2, 4, 6 */
	    tp1 = n+2;
	    if(tp1 >= ntiles) tp1 -= ntiles;
	    ip1 = nx-j-1;
	    jp1 = 0;
	  }
	}        
        if(im1 < 0) {  /* find the neighbor tile. */
	  if(n % 2 == 0) { /* tile 1, 3, 5 */
	    tm1 = n-2;
	    if(tm1 < 0) tm1 += ntiles;
	    jm1 = nx;
	    im1 = nx-j;
	  }
	  else { /* tile 2, 4, 6 */
	    tm1 = n-1;
	    im1 = nx;
	  }
	}

	angle_dx[n*nxp*nxp+j*nxp+i] = atan2(y[tp1*nxp*nxp+jp1*nxp+ip1]-y[tm1*nxp*nxp+jm1*nxp+im1],
					    (x[tp1*nxp*nxp+jp1*nxp+ip1]-x[tm1*nxp*nxp+jm1*nxp+im1])*lon_scale )*R2D;
	tp1 = n;
	tm1 = n;
	ip1 = i;
	im1 = i;
	jp1 = j+1;
	jm1 = j-1;
        if(jp1 >=nxp) {  /* find the neighbor tile. */
	  if(n % 2 == 0) { /* tile 1, 3, 5 */
	    tp1 = n+2;
	    if(tp1 >= ntiles) tp1 -= ntiles;
	    jp1 = nx-i;
	    ip1 = 0;
	  }
	  else { /* tile 2, 4, 6 */
	    tp1 = n+1;
	    if(tp1 >= ntiles) tp1 -= ntiles;
	    jp1 = 0;
	  }
	}        
        if(jm1 < 0) {  /* find the neighbor tile. */
	  if(n % 2 == 0) { /* tile 1, 3, 5 */
	    tm1 = n-1;
	    if(tm1 < 0) tm1 += ntiles;
	    jm1 = nx;
	  }
	  else { /* tile 2, 4, 6 */
	    tm1 = n-2;
	    if(tm1 < 0) tm1 += ntiles;
	    im1 = nx;
	    jm1 = nx-i;
	  }
	}	

	angle_dy[n*nxp*nxp+j*nxp+i] = atan2(y[tp1*nxp*nxp+jp1*nxp+ip1]-y[tm1*nxp*nxp+jm1*nxp+im1],
					    (x[tp1*nxp*nxp+jp1*nxp+ip1]-x[tm1*nxp*nxp+jm1*nxp+im1])*lon_scale )*R2D;
      }
    }
  }

}; /* calc_rotation_angle2 */


/* This routine calculate center location based on the vertices location */
void cell_center(int ni, int nj, const double *lonc, const double *latc, double *lont, double *latt)
{

  int    nip, njp, i, j, p, p1, p2, p3, p4;
  double *xc, *yc, *zc, *xt, *yt, *zt;
  double dd;
  
  nip = ni+1;
  njp = nj+1;
  xc = (double *)malloc(nip*njp*sizeof(double));
  yc = (double *)malloc(nip*njp*sizeof(double));
  zc = (double *)malloc(nip*njp*sizeof(double));
  xt = (double *)malloc(ni *nj *sizeof(double));
  yt = (double *)malloc(ni *nj *sizeof(double));
  zt = (double *)malloc(ni *nj *sizeof(double));  
  latlon2xyz(nip*njp, lonc, latc, xc, yc, zc); 

  for(j=0; j<nj; j++) for(i=0; i<ni; i++) {
    p =  j*ni+i;
    p1 = j*nip+i;
    p2 = j*nip+i+1;
    p3 = (j+1)*nip+i+1;
    p4 = (j+1)*nip+i;
    xt[p] = xc[p1] + xc[p2] + xc[p3] + xc[p4];
    yt[p] = yc[p1] + yc[p2] + yc[p3] + yc[p4];
    zt[p] = zc[p1] + zc[p2] + zc[p3] + zc[p4];

    dd = sqrt(pow(xt[p],2) + pow(yt[p],2) + pow(zt[p],2) );
    xt[p] /= dd;
    yt[p] /= dd;
    zt[p] /= dd;
  }
  xyz2latlon(ni*nj, xt, yt, zt, lont, latt);
  free(zt);
  free(yt);
  free(xt);
  free(zc);
  free(yc);
  free(xc);
  
}; /* cell_center */


/* This routine calculate east location based on the vertices location */
void cell_east(int ni, int nj, const double *lonc, const double *latc, double *lone, double *late)
{

  int    nip, njp, i, j, p, p1, p2;
  double *xc, *yc, *zc, *xe, *ye, *ze;
  double dd;
  
  nip = ni+1;
  njp = nj+1;
  xc = (double *)malloc(nip*njp*sizeof(double));
  yc = (double *)malloc(nip*njp*sizeof(double));
  zc = (double *)malloc(nip*njp*sizeof(double));
  xe = (double *)malloc(nip*nj *sizeof(double));
  ye = (double *)malloc(nip*nj *sizeof(double));
  ze = (double *)malloc(nip*nj *sizeof(double));  
  latlon2xyz(nip*njp, lonc, latc, xc, yc, zc); 

  for(j=0; j<nj; j++) for(i=0; i<nip; i++) {
    p =  j*nip+i;
    p1 = j*nip+i;
    p2 = (j+1)*nip+i;
    xe[p] = xc[p1] + xc[p2];
    ye[p] = yc[p1] + yc[p2];
    ze[p] = zc[p1] + zc[p2];

    dd = sqrt(pow(xe[p],2) + pow(ye[p],2) + pow(ze[p],2) );
    xe[p] /= dd;
    ye[p] /= dd;
    ze[p] /= dd;
  }
  xyz2latlon(nip*nj, xe, ye, ze, lone, late);
  free(ze);
  free(ye);
  free(xe);
  free(zc);
  free(yc);
  free(xc);
  
}; /* cell_east */


/* This routine calculate center location based on the vertices location */
void cell_north(int ni, int nj, const double *lonc, const double *latc, double *lonn, double *latn)
{

  int    nip, njp, i, j, p, p1, p2;
  double *xc, *yc, *zc, *xn, *yn, *zn;
  double dd;
  
  nip = ni+1;
  njp = nj+1;
  xc = (double *)malloc(nip*njp*sizeof(double));
  yc = (double *)malloc(nip*njp*sizeof(double));
  zc = (double *)malloc(nip*njp*sizeof(double));
  xn = (double *)malloc(ni *njp*sizeof(double));
  yn = (double *)malloc(ni *njp*sizeof(double));
  zn = (double *)malloc(ni *njp*sizeof(double));  
  latlon2xyz(nip*njp, lonc, latc, xc, yc, zc); 

  for(j=0; j<njp; j++) for(i=0; i<ni; i++) {
    p =  j*ni+i;
    p1 = j*nip+i;
    p2 = j*nip+i+1;
    xn[p] = xc[p1] + xc[p2];
    yn[p] = yc[p1] + yc[p2];
    zn[p] = zc[p1] + zc[p2];

    dd = sqrt(pow(xn[p],2) + pow(yn[p],2) + pow(zn[p],2) );
    xn[p] /= dd;
    yn[p] /= dd;
    zn[p] /= dd;
  }
  xyz2latlon(ni*njp, xn, yn, zn, lonn, latn);
  free(zn);
  free(yn);
  free(xn);
  free(zc);
  free(yc);
  free(xc);
  
}; /* cell_north */

/*-------------------------------------------------------------------------------------------
  void spherical_linear_interpolation
  This formula interpolates along the great circle connecting points p1 and p2.
  This formula is taken from http://en.wikipedia.org/wiki/Slerp and is
  attributed to Glenn Davis based on a concept by Ken Shoemake.
  -------------------------------------------------------------------------------------------*/
void spherical_linear_interpolation(double beta, const double *p1, const double *p2, double *pb)
{

  double pm[2];
  double e1[3], e2[3], eb[3];
  double dd, alpha, omega;
 
  if ( fabs(p1[0] - p2[0]) < EPSLN8 && fabs(p1[1] - p2[1]) < EPSLN8 ) {
    printf("WARNING from create_gnomonic_cubic_grid: spherical_linear_interpolation was passed two colocated points.");
    pb[0] = p1[0];
    pb[1] = p1[1];
    return ;
  }

  latlon2xyz(1, p1, p1+1, e1, e1+1, e1+2);
  latlon2xyz(1, p2, p2+1, e2, e2+1, e2+2);

  dd = sqrt( e1[0]*e1[0] + e1[1]*e1[1] + e1[2]*e1[2]);

  e1[0] /= dd;
  e1[1] /= dd;
  e1[2] /= dd;

  dd = sqrt( e2[0]*e2[0] + e2[1]*e2[1] + e2[2]*e2[2]);
  e2[0] /= dd;
  e2[1] /= dd;
  e2[2] /= dd;

  alpha = 1. - beta;

  omega = acos( e1[0]*e2[0] + e1[1]*e2[1] + e1[2]*e2[2] );

  if ( fabs(omega) < EPSLN5 ) {
    printf("spherical_linear_interpolation: omega=%g, p1 = %g,%g, p2 = %g,%g\n",
	   omega, p1[0], p1[1], p2[0], p2[1]);
    mpp_error("spherical_linear_interpolation: interpolation not well defined between antipodal points");
  }

  eb[0] = sin( beta*omega )*e2[0] + sin(alpha*omega)*e1[0];
  eb[1] = sin( beta*omega )*e2[1] + sin(alpha*omega)*e1[1];
  eb[2] = sin( beta*omega )*e2[2] + sin(alpha*omega)*e1[2];

  eb[0] /= sin(omega);
  eb[1] /= sin(omega);
  eb[2] /= sin(omega);
  
  xyz2latlon(1, eb, eb+1, eb+2, pb, pb+1);
}



/* void setup_aligned_nest

/*

parent_tile: tile number of parent grid.
nx_parent  : parent grid size in x-direction.
ny_parent  : parent grid size in y-direction.


*/


void setup_aligned_nest(int parent_tile, int parent_ni, int parent_nj,
			const double *parent_xc, const double *parent_yc, int halo,
			int refine_ratio, int istart_nest, int iend_nest, int jstart_nest, int jend_nest,
			double *x, double *y, double *dx, double *dy,
			double *area, double *angle_dx, double *angle_dy)	
{
  double q1[2], q2[2], t1[2], t2[2], p1[0], p2[0];
  double two_pi;
  int ni, nj, npi, npj, nx, ny, npx, npy;
  int  parent_npi, i, j, ic, jc, imod, jmod;
  double *xc=NULL, *yc=NULL;
  double *xt=NULL, *yt=NULL;
  double *xe=NULL, *ye=NULL;
  double *xn=NULL, *yn=NULL;
  int    istart, iend, jstart, jend;
  
  two_pi = 2.*M_PI;
  /* istart_nest, iend_nest, jstart_nest, jend_nest are the index on the parent supergrid.
     transfter them on to model grid.
  */
  if( (istart_nest+1)%2 ) mpp_error("create_gnomonic_cubic_grid(setup_aligned_nest): istart_nest+1 is not divisbile by 2");
  if( iend_nest%2 ) mpp_error("create_gnomonic_cubic_grid(setup_aligned_nest): iend_nest is not divisbile by 2");
  if( (jstart_nest+1)%2 ) mpp_error("create_gnomonic_cubic_grid(setup_aligned_nest): jstart_nest+1 is not divisbile by 2");
  if( jend_nest%2 ) mpp_error("create_gnomonic_cubic_grid(setup_aligned_nest): jend_nest is not divisbile by 2");
  
  istart = (istart_nest+1)/2;
  iend   = iend_nest/2;
  jstart = (jstart_nest+1)/2;
  jend   = jend_nest/2;
    
  /* Check that the grid does not lie outside its parent */
  if( jstart + floor( (-halo)/(double)refine_ratio ) < 1 ||
      istart + floor( (-halo)/(double)refine_ratio ) < 1 ||
      jend + ceil( halo/(double)refine_ratio ) > parent_nj ||
      iend + ceil( halo/(double)refine_ratio ) > parent_ni )
    mpp_error("create_gnomonic_cubic_grid(setup_aligned_nest): nested grid lies outside its parent");

  ni = (iend-istart+1)*refine_ratio;
  nj = (jend-jstart+1)*refine_ratio;
  npi = ni+1;
  npj = nj+1;
  nx = 2*ni;
  ny = 2*nj;
  npx = nx+1;
  npy = ny+1;

  xc = (double *)malloc(npi*npj*sizeof(double));
  yc = (double *)malloc(npi*npj*sizeof(double));
  xt = (double *)malloc(ni *nj *sizeof(double));
  yt = (double *)malloc(ni *nj *sizeof(double));
  xe = (double *)malloc(npi*nj *sizeof(double));
  ye = (double *)malloc(npi*nj *sizeof(double));
  xn = (double *)malloc(ni *npj*sizeof(double));
  yn = (double *)malloc(ni *npj*sizeof(double));
  
  parent_npi = parent_ni+1;
  
  for(j=0; j<npj; j++) {
    jc = jstart - 1 + j/refine_ratio;
    jmod = j%refine_ratio;
    for(i=0; i<npi; i++) {
      ic = istart - 1 + i/refine_ratio;
      imod = i%refine_ratio;

      if(jmod == 0) {
	q1[0] = parent_xc[jc*parent_npi+ic];
	q1[1] = parent_yc[jc*parent_npi+ic];
	q2[0] = parent_xc[jc*parent_npi+ic+1];
	q2[1] = parent_yc[jc*parent_npi+ic+1];
      }
      else {
	t1[0] = parent_xc[jc*parent_npi+ic];
	t1[1] = parent_yc[jc*parent_npi+ic];
	t2[0] = parent_xc[(jc+1)*parent_npi+ic];
	t2[1] = parent_yc[(jc+1)*parent_npi+ic];
	spherical_linear_interpolation( (double)jmod/refine_ratio, t1, t2, q1);
	t1[0] = parent_xc[jc*parent_npi+ic+1];
	t1[1] = parent_yc[jc*parent_npi+ic+1];
	t2[0] = parent_xc[(jc+1)*parent_npi+ic+1];
	t2[1] = parent_yc[(jc+1)*parent_npi+ic+1];	
	spherical_linear_interpolation( (double)jmod/refine_ratio, t1, t2, q2);
      }

      if (imod == 0) {
	xc[j*npi+i] = q1[0];
	yc[j*npi+i] = q1[1];
      }
      else {
	spherical_linear_interpolation( (double)imod/refine_ratio, q1, q2, t1 );
	xc[j*npi+i] = t1[0];
	yc[j*npi+i] = t1[1];
      }

      if( xc[j*npi+i] > two_pi ) xc[j*npi+i] -= two_pi;
      if( xc[j*npi+i] < 0. ) xc[j*npi+i] += two_pi;
    }
  }

  /* calculate grid box center location */
  cell_center(ni, nj, xc, yc, xt, yt);
  cell_east  (ni, nj, xc, yc, xe, ye);
  cell_north (ni, nj, xc, yc, xn, yn);
  /* copy grid box vertices location into super grid */
  for(j=0; j<npj; j++) for(i=0; i<npi; i++) {
    x[j*2*npx+i*2]=xc[j*npi+i];
    y[j*2*npx+i*2]=yc[j*npi+i];
  }

  /* copy grid box center location to super grid */
  for(j=0; j<nj; j++) for(i=0; i<ni; i++) {
    x[(j*2+1)*npx+i*2+1]=xt[j*ni+i];
    y[(j*2+1)*npx+i*2+1]=yt[j*ni+i];
  }  

  /* copy grid box east location to super grid */
  for(j=0; j<nj; j++) for(i=0; i<npi; i++) {
    x[(j*2+1)*npx+i*2]=xe[j*npi+i];
    y[(j*2+1)*npx+i*2]=ye[j*npi+i];
  }  

  /* copy grid box north location to super grid */
  for(j=0; j<npj; j++) for(i=0; i<ni; i++) {
    x[j*2*npx+i*2+1]=xn[j*ni+i];
    y[j*2*npx+i*2+1]=yn[j*ni+i];
  }    
  
  /* calculate grid cell length */
  for(j=0; j<npy; j++) {
    for(i=0; i<nx; i++) {
      p1[0] = x[j*npx+i];
      p1[1] = y[j*npx+i];
      p2[0] = x[j*npx+i+1];
      p2[1] = y[j*npx+i+1];
      dx[j*nx+i] = great_circle_distance(p1, p2);
    }
  }

  for(j=0; j<ny; j++) {
    for(i=0; i<npx; i++) {
      p1[0] = x[j*npx+i];
      p1[1] = y[j*npx+i];
      p2[0] = x[(j+1)*npx+i];
      p2[1] = y[(j+1)*npx+i];
      dy[j*nx+i] = great_circle_distance(p1, p2);
    }
  }
  
  calc_cell_area(nx, ny, x, y, area);

  /* The rotation angle is not used in the model and the computation on the edge id complicated,
     so currently just set the rotation angle to be 0
  */
  for(i=0; i<npx*npy; i++) {
    angle_dx[i] = 0;
    angle_dy[i] = 0;
  }
  

  /* convert grid location from radians to degree */
  for(i=0; i<npx*npy; i++) {
    x[i] = x[i]*R2D;
    y[i] = y[i]*R2D;
  }
  free(xt);
  free(yt);
  free(xc);
  free(yc);  
  free(xe);
  free(ye);
  free(xn);
  free(yn);  

}

