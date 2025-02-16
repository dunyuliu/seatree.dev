/*
* Copyright (C) 1999-2005 Anthony Lomax <anthony@alomax.net, http://www.alomax.net>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


/*   NLLoc.c

	Program to do global search earthquake location in 3-D models

*/


/*-----------------------------------------------------------------------
Anthony Lomax
Anthony Lomax Scientific Software
161 Allee du Micocoulier, 06370 Mouans-Sartoux, France
tel: +33(0)493752502  e-mail: anthony@alomax.net  web: http://www.alomax.net
-------------------------------------------------------------------------*/


/*
	history:	(see also http://alomax.net/nlloc -> Updates)

	ver 01    26SEP1997  AJL  Original version
	ver 02    08JUN1998  AJL  Metropolis added
	ver  2         2000  AJL  Oct-Tree added
	ver  3      DEC2003  AJL  EDT added
	ver  4    10MAY2004  AJL  Added following changes from S. Husen:
		27MAR2002  *SH   VELEST phase format added (changes in GetNextObservation)
		28AUG2002  *SH   event origin time is now calculated relative to
				the second (and not minute as before); initial
				OT seconds are now read in and added to arrival
				time (changes in GetNextObservation)
		17NOV2002  *SH   UUSS phase format added (changes in GetObservations
				and GetNextObservation)
		12JUN2003  *SH   Fixed bug with arrival times > 100s in UUSS phase format
		01OCT2003  *SH   added SED format "SED_LOC"
				code was written by A. Lomax; bug fix by Danijel Schorlemmer
		02MAR2004  *SH   modifications to make NLLoc compatible for routine earthquake
				location with SNAP (SED):
				- introduced 2nd argument snap_pid, which is the snap_pid of SNAP; needed
				to form filename of outputfile hyprint{snap_pid}; usage of NLLoc
				now is:
				NLLoc <control file> <snap_pid>
				- added new subroutine WriteSnapSum: output location results
				into file hyprint{snap_pid} in format readable by SNAP; format
				is identical to output format of program grid_search by
				M. Baer of SED;
				file hyprint{snap_pid} will be written if control parameter
				LOCHYPOUT is set to SAVE_SNAP_SUM
				- added subroutine get_region_names_nr and associated subroutines
				to convert lat/lon into Swiss coordinates in km and to find
				region name for local earthquakes in Switzerland
		NOV2004  AJL   Split off NLLocLib and created NLDiffLoc (non-linear double-difference location)
		APR2005  AJL   Added LOCSTAWT, LOCELEVCORR, LOCDELAY_SURFACE
		JAN2006 Frederik Tilmann    Changes to SEISAN reader
			- 5 character station names now permitted
			- checks format line code in column 80 to only read phase lines.
				Previously some 'unlucky' lines
				of other types got interpreted as phase lines
			- can now read high precision phase times (accurate to 0.001 s) .
				Previously these high precision
				picks drops 10s of seconds, resulting in completely wrong times
			- now read instrument and component
		APR2006  AJL   Added  LOCMETH-EDT_OT_WT, LOCSEARCH-OCT:useStationsDensity, stopOnMinNodeSize


.........1.........2.........3.........4.........5.........6.........7.........8

*/


/* References */
/*
	TV82	Tarantola and Valette,  (1982)
		"Inverse Problems = Quest for Information",
		J Geophys 50, 159-170.
	MEN92	Moser, van Eck and Nolet,  (1992)
		"Hypocenter Determination ... Shortest Path Method",
		JGR 97, B5, 6563-6572.
*/





//#include "GridLib.h"
//#include "ran1.h"
//#include "velmod.h"
#include "NLLocLib.h"
//#include "GridMemLib.h"
//#include "calc_crust_corr.h"





/** function to perform grid search location */

int Locate(int ngrid, char* fn_root_out, int numArrivalsReject, int return_locations, int return_oct_tree_grid, int return_scatter_sample, LocNode **ploc_list_head)
{

	int istat, n, narr;
	char fnout[FILENAME_MAX];

	FILE *fpio;
	char fname[FILENAME_MAX];
	float *fdata = NULL;
	float ftemp;
	int iSizeOfFdata;
	double alpha_2;
	double oct_node_value_max, oct_tree_integral = 0.0;
	double oct_tree_prob_integral = 0.0;

	// AJL 20071219
	Location *ploc_list_node;


	Hypocenter.nScatterSaved = -1;


	/* write message */

	putmsg(2, "");
	if (SearchType == SEARCH_GRID)
		sprintf(MsgStr, "Searching Grid %d:", ngrid);
	else if (SearchType == SEARCH_MET)
		sprintf(MsgStr, "Applying Metropolis within Grid %d:", ngrid);
	else if (SearchType == SEARCH_OCTTREE)
		sprintf(MsgStr, "Applying Octtree search within Grid %d:", ngrid);
	putmsg(2, MsgStr);
	if (message_flag >= 3)
		display_grid_param(LocGrid + ngrid);



	/* set output name */
	sprintf(fnout, "%s.grid%d", fn_root_out, ngrid);
	strcpy(Hypocenter.fileroot, fnout);

	/* initialize hypocenter fields */
	sprintf(Hypocenter.locStat, "LOCATED");
	sprintf(Hypocenter.locStatComm, "Location completed.");
	Hypocenter.x = Hypocenter.y = Hypocenter.z = 0.0;
	Hypocenter.ix = Hypocenter.iy = Hypocenter.iz = -1;


	/* search type dependent initializations */

	if (SearchType == SEARCH_GRID) {

		/* check that current grid is contained within first grid */

		if (!IsGridInside(LocGrid + ngrid, LocGrid, 0)) {
			puterr(
	"WARNING: this grid not entirely contained inside 0th grid, ending search for this event.");
			return(GRID_NOT_INSIDE);
		}

		/* initialize 3D location grid */

		/* allocate location grid */
		LocGrid[ngrid].buffer = AllocateGrid(LocGrid + ngrid);
		if (LocGrid[ngrid].buffer == NULL) {
			puterr(
	"ERROR: allocating memory for 3D location grid buffer.");
			return(EXIT_ERROR_MEMORY);
		}
		/* create array access pointers */
		LocGrid[ngrid].array = CreateGridArray(LocGrid + ngrid);
		if (LocGrid[ngrid].array == NULL) {
			puterr(
	"ERROR: creating array for accessing 3D location grid buffer.");
			return(EXIT_ERROR_MEMORY);
		}
		LocGrid[ngrid].sum = 0.0;


		/* reset y-z dual-sheet grids (3D time grids) */

		for (narr = 0; narr < NumArrivalsLocation; narr++) {
			if (Arrival[narr].sheetdesc.type == GRID_TIME)
				Arrival[narr].sheetdesc.origx =
						VERY_LARGE_DOUBLE;
		}



	} else if (SearchType == SEARCH_MET) {

/* test change 17JAN2000 AJL */
/*		InitializeMetropolisWalk(LocGrid + ngrid ,
			Arrival, NumArrivalsLocation, &Metrop,
			MetNumSamples, MetStepInit);
*/

		InitializeMetropolisWalk(LocGrid + ngrid ,
			Arrival, NumArrivalsLocation, &Metrop,
			MetLearn + MetEquil, MetStepInit);

		/* allocate scatter array for saved samples */
		iSizeOfFdata = (1 + MetUse / MetSkip) * 4 * sizeof(float);
		if ((fdata = (float *) malloc(iSizeOfFdata)) == NULL)
			return(EXIT_ERROR_LOCATE);
		NumAllocations++;

	} else if (SearchType == SEARCH_OCTTREE) {

		// station denisty weighting
		if (octtreeParams.use_stations_density) {
			AveInterStationDistance = calcAveInterStationDistance(StationList, NumStations);
			sprintf(MsgStr, "Station Density Weight:  Ave Station Distance: %lf", AveInterStationDistance);
			putmsg(1, MsgStr);
			if (AveInterStationDistance < SMALL_DOUBLE) {	// should not get here
				puterr("ERROR: cannot apply OctTree Station Density Weight: Ave Station Distance is zero!");
			}
			NumForceOctTreeStaDenWt = 0;
		}

		// initialize memory/arrays for regular, initial oct-tree search grid
		// this is an x, y, z array of octtree root nodes,
		// a true oct-tree is created at each of these roots
		octTree = InitializeOcttree(LocGrid + ngrid, &octtreeParams);
		NumAllocations++;

		// allocate scatter array for saved samples
		iSizeOfFdata = octtreeParams.num_scatter * 4 * sizeof(float);
		iSizeOfFdata = (12 * iSizeOfFdata) / 10; // sample may be slightly larger than requested
		if ((fdata = (float *) malloc(iSizeOfFdata)) == NULL)
			return(EXIT_ERROR_LOCATE);
		NumAllocations++;

	}


	/* since sorted, reset companion indices */
	if (VpVsRatio > 0.0) {
		//for (narr = 0; narr < NumArrivalsLocation; narr++) {
		for (narr = 0; narr < NumArrivals; narr++) {
			if (Arrival[narr].n_companion < 0)
				continue;
			if (IsPhaseID(Arrival[narr].phase, "S") &&
					(Arrival[narr].n_companion =
					IsSameArrival(Arrival, narr, narr, "P")) < 0) {
				puterr("ERROR: cannot find companion arrival.");
				return(EXIT_ERROR_LOCATE);
			}
		}
	}


	/* do search */

	if (SearchType == SEARCH_GRID) {

		/* grid-search location (fill location grid) */
		if ((istat =
			LocGridSearch(ngrid, NumArrivals, NumArrivalsLocation,
				Arrival, LocGrid + ngrid, &Gauss, &Hypocenter)) < 0) {
			puterr("ERROR: in grid search location.");
			return(EXIT_ERROR_LOCATE);
		}

	} else if (SearchType == SEARCH_MET) {

		/* Metropolis location (random walk) */
		if ((Hypocenter.nScatterSaved =
			LocMetropolis(ngrid, NumArrivals, NumArrivalsLocation,
				Arrival, LocGrid + ngrid,
				&Gauss, &Hypocenter, &Metrop, fdata)) < 0) {
			puterr("ERROR: in Metropolis location.");
			return(EXIT_ERROR_LOCATE);
		}

	} else if (SearchType == SEARCH_OCTTREE) {

		/* do Octree location (importance sampling) */
		if ((Hypocenter.nScatterSaved =
			LocOctree(ngrid, NumArrivals, NumArrivalsLocation,
				Arrival, LocGrid + ngrid,
				&Gauss, &Hypocenter, &octtreeParams,
				octTree, fdata, &oct_node_value_max, &oct_tree_integral)) < 0) {
			puterr("ERROR: in Octree location.");
			return(EXIT_ERROR_LOCATE);
		}

	}



	/* clean up dates, caclulate rms */
	StdDateTime(Arrival, NumArrivals, &Hypocenter);

	/* determine azimuth gap */
	Hypocenter.gap = CalcAzimuthGap(Arrival, NumArrivalsLocation);

	/* re-sort arrivals by distance */
	if ((istat = SortArrivalsDist(Arrival, NumArrivals)) < 0) {
		puterr("ERROR: sorting arrivals by distance.");
		return(EXIT_ERROR_LOCATE);
	}

	/* save distance to closest station */
	Hypocenter.dist = Arrival->dist;


	/* search type dependent processing */

	alpha_2 = 3.53;  /* value for 68% conf
				(see Num Rec, 2nd ed, sec 15.6) */
	if (SearchType == SEARCH_GRID && LocGridSave[ngrid]) {

		/* calculate confidence intervals and save to disk */

		if (LocGrid[ngrid].type == GRID_PROB_DENSITY)  {
			if ((istat = CalcConfidenceIntrvl(LocGrid + ngrid,
					&Hypocenter, fnout)) < 0) {
				puterr("ERROR: calculating confidence intervals.");
				return(EXIT_ERROR_LOCATE);
			}

			/* generate probabilistic scatter of events */

			if ((istat = GenEventScatterGrid(LocGrid + ngrid,
					&Hypocenter, &Scatter, fnout)) < 0) {
				puterr("ERROR: calculating event scatter.");
			}

			/* calculate "traditional" statistics */
			Hypocenter.expect = CalcExpectation(LocGrid + ngrid, NULL);
			istat = rect2latlon(0, Hypocenter.expect.x, Hypocenter.expect.y,
				&(Hypocenter.expect_dlat), &(Hypocenter.expect_dlong));
			Hypocenter.cov = CalcCovariance(LocGrid + ngrid, &Hypocenter.expect, NULL);
			Hypocenter.ellipsoid = CalcErrorEllipsoid(&Hypocenter.cov, alpha_2);

		} else {
			Hypocenter.probmax = -1.0;
		}

	} else if ((SearchType == SEARCH_MET || SearchType == SEARCH_OCTTREE ) && LocGridSave[ngrid]) {

		if (octtreeParams.use_stations_density) {
			sprintf(MsgStr, "Station Density Weight:  Number Force Divide: %d  max_num_nodes: %d", NumForceOctTreeStaDenWt, octtreeParams.max_num_nodes);
			putmsg(1, MsgStr);
			if (NumForceOctTreeStaDenWt >= octtreeParams.max_num_nodes) {
				puterr("ERROR: Number Force Divide > max_num_nodes !  Must reduce LOCSEARCH use_stations_density level.");
			}
			else if (NumForceOctTreeStaDenWt > (9 * octtreeParams.max_num_nodes) / 10) {
				puterr("WARNING: Number Force Divide > 90% max_num_nodes !  Should reduce LOCSEARCH use_stations_density level.");
			}
		}


		if (SearchType == SEARCH_OCTTREE) {

			/*
			// determine integral of all oct-tree leaf node pdf values
			oct_tree_integral = integrateResultTree(resultTreeRoot, 0.0, oct_node_value_max);
			sprintf(MsgStr, "Octree oct_node_value_max= %le oct_tree_integral= %le", oct_node_value_max, oct_tree_integral);
			putmsg(1, MsgStr);*/

			// generate scatter sample
			if (Hypocenter.nScatterSaved == 0) // not saved during search
				Hypocenter.nScatterSaved = GenEventScatterOcttree(&octtreeParams, oct_node_value_max, fdata, oct_tree_integral, &Hypocenter);

		}

		/* write scatter file */
		if (iSaveNLLocEvent) {
			sprintf(fname, "%s.loc.scat", fnout);
			if ((fpio = fopen(fname, "w")) != NULL) {
				/* write scatter file header informaion */
				fseek(fpio, 0, SEEK_SET);
				fwrite(&(Hypocenter.nScatterSaved), sizeof(int), 1, fpio);
				ftemp = (float) Hypocenter.probmax;
				fwrite(&ftemp, sizeof(float), 1, fpio);
				/* skip header record */
				fseek(fpio, 4 * sizeof(float), SEEK_SET);
				/* write scatter samples */
				fwrite(fdata, 4 * sizeof(float), Hypocenter.nScatterSaved, fpio);
				fclose(fpio);
			} else {
				puterr("ERROR: opening scatter output file.");
				return(EXIT_ERROR_IO);
			}
		}

		if (SearchType == SEARCH_OCTTREE && (iSaveNLLocOctree || return_oct_tree_grid)) {

			if (LocGrid[ngrid].type == GRID_PROB_DENSITY)  {
				// convert oct tree values to likelihood
				oct_tree_prob_integral = convertOcttreeValuesToProb(resultTreeRoot, 0.0, oct_node_value_max);
				octTree->data_code = GRID_LIKELIHOOD;
				octTree->integral = oct_tree_prob_integral;
				// create new result tree sorted by node values only, without multiplication by volume
//				resultTreeLikelihoodRoot = NULL;
//				resultTreeLikelihoodRoot = createResultTree(resultTreeRoot, resultTreeLikelihoodRoot);
				sprintf(MsgStr, "Oct tree structure converted to probability.");
				putmsg(1, MsgStr);
				// convert oct tree values to confidence
				//convertOcttreeValuesToConfidence(resultTreeRoot, 0.0);
			}

			if (iSaveNLLocOctree) {
				// write oct tree structure to file
				sprintf(fname, "%s.loc.octree", fnout);
				if ((fpio = fopen(fname, "w")) != NULL) {
					istat = writeTree3D(fpio, octTree);
					fclose(fpio);
					sprintf(MsgStr, "Oct tree structure written to file : %d nodes", istat);
					putmsg(1, MsgStr);
				} else {
					puterr("ERROR: opening oct tree structure output file.");
					return(EXIT_ERROR_IO);
				}
			}
		}

		/* calculate "traditional" statistics */
		Hypocenter.expect =
			CalcExpectationSamples(fdata, Hypocenter.nScatterSaved);
		istat = rect2latlon(0, Hypocenter.expect.x, Hypocenter.expect.y,
			&(Hypocenter.expect_dlat), &(Hypocenter.expect_dlong));
		Hypocenter.cov = CalcCovarianceSamples(fdata, Hypocenter.nScatterSaved,
			&Hypocenter.expect);
		if (Hypocenter.nScatterSaved) {
			Hypocenter.ellipsoid = CalcErrorEllipsoid(&Hypocenter.cov, alpha_2);
			sprintf(MsgStr, "ellipsoid_volume = %le", (4.0 / 3.0 ) * cPI * 8.0 * Hypocenter.ellipsoid.len1 * Hypocenter.ellipsoid.len2 * Hypocenter.ellipsoid.len3);
			putmsg(1, MsgStr);
		}
	}





	/* search type dependent results saving */

	if (SearchType == SEARCH_GRID) {

		/* save location grid to disk */

		if (!iSaveNone && LocGridSave[ngrid])
			if ((istat = WriteGrid3dBuf(LocGrid + ngrid, NULL,
					fnout, "loc")) < 0) {
				puterr("ERROR: writing location grid to disk.");
				return(EXIT_ERROR_IO);
			}
	} else if (SearchType == SEARCH_MET || SearchType == SEARCH_OCTTREE) {

		/* save location grid header to disk */

		if (!iSaveNone && LocGridSave[ngrid])
			if ((istat = WriteGrid3dHdr(LocGrid + ngrid, NULL, fnout, "loc")) < 0) {
				puterr("ERROR: writing grid header to disk.");
				return(EXIT_ERROR_IO);
			}
	}



	/* display and save minimum misfit location to file */

	if (LocGridSave[ngrid]) {
		/* calculate magnitudes */
		Hypocenter.amp_mag = MAGNITUDE_NULL;
		Hypocenter.num_amp_mag = 0;
		Hypocenter.dur_mag = MAGNITUDE_NULL;
		Hypocenter.num_dur_mag = 0;
		for (n = 0; n < MAX_NUM_MAG_METHODS; n++)
			CalculateMagnitude(&Hypocenter, Arrival, NumArrivals,
				Component, NumCompDesc, Magnitude + n);
		/* calculate estimated VpVs ratio */
		CalculateVpVsEstimate(&Hypocenter, Arrival, NumArrivals);
		/* save location */
		if ((istat = SaveLocation(&Hypocenter, ngrid, fnout, numArrivalsReject, "grid", 1, &Gauss)) < 0)
			return(istat);
		/* add location to loclist */
		if (return_locations) {
			ploc_list_node = newLocation(
			cloneHypoDesc(&Hypocenter),
			cloneArrivalDescArray(Arrival, NumArrivals),
			NumArrivals, cloneGridDesc(&(LocGrid[ngrid])),
			return_oct_tree_grid ? octTree : NULL,
			return_scatter_sample ? fdata : NULL);
			*ploc_list_head = addLocationToLocList(ploc_list_head, ploc_list_node, NumEventsLocated);
		}
		/* update station statistics table */
		if (strncmp(Hypocenter.locStat, "LOCATED", 7) == 0
			&& Hypocenter.rms <= RMS_Max
			&& Hypocenter.nreadings >= NRdgs_Min
			&& Hypocenter.gap <= Gap_Max
			&& Hypocenter.ellipsoid.len3 <= Ell_Len3_Max
			&& Hypocenter.z >= Hypo_Depth_Min
				  && Hypocenter.z <= Hypo_Depth_Max) {
				UpdateStaStat(ngrid, Arrival, NumArrivals, P_ResidualMax, S_ResidualMax);
//printf("INSTALLED in Stat Table: ");
				  } else {
//printf("NOT INSTALLED in Stat Table: ");
				  }
//printf("Hypo: %s %f %d %d %f %f\n", Hypocenter.locStat, Hypocenter.rms, Hypocenter.nreadings, Hypocenter.gap, Hypocenter.ellipsoid.len3, Hypocenter.z);
	}



	/* search type dependent cleanup */

	if (SearchType == SEARCH_GRID) {

		/* free grid memory */

		DestroyGridArray(LocGrid + ngrid);
		FreeGrid(LocGrid + ngrid);

		/* intialize next grid origin location */

		if (ngrid < NumLocGrids - 1) {
			if (LocGrid[ngrid + 1].autox)
				LocGrid[ngrid + 1].origx = Hypocenter.x
				- 0.5 * (double) (LocGrid[ngrid + 1].numx - 1)
						* LocGrid[ngrid + 1].dx;
			if (LocGrid[ngrid + 1].autoy)
				LocGrid[ngrid + 1].origy = Hypocenter.y
				- 0.5 * (double) (LocGrid[ngrid + 1].numy - 1)
						* LocGrid[ngrid + 1].dy;
			if (LocGrid[ngrid + 1].autoz)
				LocGrid[ngrid + 1].origz = Hypocenter.z
				- 0.5 * (double) (LocGrid[ngrid + 1].numz - 1)
						* LocGrid[ngrid + 1].dz;

			/* try to make sure new grid is inside initial grid */
			if (!IsGridInside(LocGrid + ngrid + 1, LocGrid, 1))
				puterr(
"WARNING: cannot get next grid entirely contained inside 0th grid.");
		}

	} else if (SearchType == SEARCH_MET) {

		/* free saved samples memory */
		if (!return_scatter_sample) {
			free(fdata);
			NumAllocations--;
		}

	} else if (SearchType == SEARCH_OCTTREE) {

		// free results tree - IMPORTANT!
		freeResultTree(resultTreeRoot);

		/* free octree memory */
		if (!return_oct_tree_grid) {
			freeTree3D(octTree, 1);
			NumAllocations--;
		}

		/* free saved samples memory */
		if (!return_scatter_sample) {
			free(fdata);
			NumAllocations--;
		}

	}



	/* re-sort to get location arrivals in time order */

	if ((istat =
		SortArrivalsIgnore(Arrival, NumArrivals)) < 0) {
			puterr(
			"ERROR: sorting arrivals by ignore flag.");
		return(EXIT_ERROR_LOCATE);
	}
	if ((istat = SortArrivalsTime(Arrival, NumArrivalsLocation)) < 0) {
		puterr("ERROR: sorting arrivals by time.");
		return(EXIT_ERROR_LOCATE);
	}



	/* search type dependent return */

	if (SearchType == SEARCH_GRID) {

		return(0);

	} else if (SearchType == SEARCH_MET || SearchType == SEARCH_OCTTREE) {

		if (Hypocenter.nScatterSaved == 0)
			return(1);

		return(0);
	}

	return(0);


}





/** function to initialize Metropolis walk */

void InitializeMetropolisWalk(GridDesc* ptgrid, ArrivalDesc* parrivals, int
		numArrLoc, WalkParams* pMetrop, int numSamples, double initStep)
{
	int narr;
	double xlen, ylen, zlen, dminlen;
	double xmin, xmax, ymin, ymax;
	SourceDesc* pstation;


	/* set walk limits equal to grid limits */
	xmin = ptgrid->origx;
	xmax = xmin + (double) (ptgrid->numx - 1) * ptgrid->dx;
	ymin = ptgrid->origy;
	ymax = ymin + (double) (ptgrid->numy - 1) * ptgrid->dy;

	/* find station with earliest arrival and non-zero weight */
	narr = 0;
	while(narr < numArrLoc && parrivals[narr].weight < 0.001)
		narr++;

	/* initialize walk location */
	if (narr < numArrLoc)
		pstation = &(parrivals[narr].station);
	if (narr < numArrLoc &&
		   pstation->x >= xmin && pstation->x <= xmax
		   && pstation->y >= ymin && pstation->y <= ymax) {
		/* start walk at location of station with earliest arrival */
		pMetrop->x = pstation->x;
		pMetrop->y = pstation->y;
		   } else {
			   /* start walk at grid center */
			   pMetrop->x = ptgrid->origx
					   + (double) (ptgrid->numx - 1) * ptgrid->dx / 2.0;
			   pMetrop->y = ptgrid->origy
					   + (double) (ptgrid->numy - 1) * ptgrid->dy / 2.0;
		   }
		   /* start walk at grid center depth */
		   pMetrop->z = ptgrid->origz
				   + (double) (ptgrid->numz - 1) * ptgrid->dz / 2.0;

		   /* calculate initial step size */
		   if (initStep < 0.0) {
			   xlen = (double) ptgrid->numx * ptgrid->dx / 2.0;
			   ylen = (double) ptgrid->numy * ptgrid->dy / 2.0;
			   zlen = (double) ptgrid->numz * ptgrid->dz / 2.0;
			   dminlen = xlen < ylen ? xlen : ylen;
			   dminlen = dminlen < zlen ? dminlen : zlen;
			   /* step is size that tiles plane parallel to max len sides */
			   pMetrop->dx = sqrt((xlen * ylen * zlen / dminlen) / (double) numSamples);
			   /* step is size that tiles search volume */
			   /*pMetrop->dx = pow(xlen * ylen * zlen / (double) numSamples, 1.0/3.0);*/
		   } else {
			   pMetrop->dx = initStep;
		   }

		   if (message_flag >= 4) {
			   sprintf(MsgStr,
				   "INFO: Metropolis initial step size: %lf", pMetrop->dx);
			   putmsg(4, MsgStr);
		   }

		   /* set likelihood */
		   pMetrop->likelihood = -1.0;

}



/** function to display and save minimum misfit location to file */

int SaveLocation(HypoDesc* hypo, int ngrid, char *fnout, int numArrivalsReject,
		 char* typename, int isave_phases, GaussLocParams* gauss_par)
{
	int istat;
	char *pchr;
	char sys_command[MAXLINE_LONG];
	char fname[FILENAME_MAX], frootname[FILENAME_MAX];
	FILE *fp_tmp;

	/* set signature string */
	sprintf(hypo->signature, "%s   %s:v%s %s",
		LocSignature, prog_name, PVER, CurrTimeStr());
	while((pchr = strchr(hypo->signature, '\n')))
		*pchr = ' ';

	/* display hypocenter to std out */
	if (message_flag >= 3)
		WriteLocation(stdout, hypo, Arrival,
			      NumArrivals + numArrivalsReject, fnout,
			      isave_phases, 1, 0, LocGrid + ngrid, 0);

	/*  save requested hypocenter/phase formats */

#ifdef CUSTOM_ETH
/* SH 02/26/2004
	added new routine WriteSnapSum to write hypocenter summary to file (SNAP format) */
// must call WriteSnapSum before other output because it sets ETH magnitudes
	if (iSaveSnapSum) {
		WriteSnapSum(NULL, hypo, Arrival, NumArrivals);
	}
#endif

	if (iSaveNLLocEvent) {
		/* write NLLoc hypocenter to event file */
		sprintf(frootname, "%s.loc", fnout);
		sprintf(fname, "%s.hyp", frootname);
		if ((istat = WriteLocation(NULL, hypo, Arrival,
		     NumArrivals + numArrivalsReject, fname, isave_phases, 1, 0,
		     LocGrid + ngrid, 0)) < 0) {
			     puterr("ERROR: writing location to event file.");
			     return(EXIT_ERROR_IO);
		}
		/* copy event file to last.hyp */
		sprintf(sys_command, "cp %s %slast.hyp", fname, f_outpath);
		system(sys_command);
		sprintf(fname, "%s.hdr", frootname);
		sprintf(sys_command, "cp %s %slast.hdr", fname, f_outpath);
		system(sys_command);
		sprintf(fname, "%s.scat", frootname);
		if ((fp_tmp = fopen(fname, "r")) != NULL) {
			fclose(fp_tmp);
			sprintf(sys_command, "cp %s %slast.scat", fname, f_outpath);
			system(sys_command);
		}
	}

	if (iSaveNLLocSum) {
		/* write NLLoc hypocenter to summary file */
		if ((istat = WriteLocation(pSumFileHypNLLoc[ngrid],
		     hypo,
		     Arrival, NumArrivals, fnout, 0, 1, 0,
		     LocGrid + ngrid, 0)) < 0) {
			     puterr("ERROR: writing location to summary file.");
			     return(EXIT_ERROR_IO);
		     }
		     /* copy event grid header to .sum header */
		     sprintf(sys_command,
			     "cp %s.loc.hdr %s.sum.%s%d.loc.hdr",
			     fnout, fn_path_output, typename, ngrid);
		     system(sys_command);
	}


	if (iSaveHypo71Event) {
		/* write HYPO71 hypocenter to event file */
		WriteHypo71(NULL, hypo, Arrival, fnout, 1, 1);
	}
	if (iSaveHypo71Sum) {
		/* write HYPO71 hypocenter to summary file */
		WriteHypo71(pSumFileHypo71[ngrid], hypo,
			    Arrival, fnout, iWriteHypHeader[ngrid], 0);
	}

	if (iSaveHypoEllEvent) {
		/* write pseudo-HypoEllipse hypo to event file */
		WriteHypoEll(NULL, hypo, Arrival, fnout, 1, 1);
	}
	if (iSaveHypoEllSum) {
		/* write pseudo-HypoEllipse hypo to summary file */
		WriteHypoEll(pSumFileHypoEll[ngrid], hypo,
			     Arrival, fnout, iWriteHypHeader[ngrid], 0);
	}

	if (iSaveHypoInvSum) {
		/* write HypoInverseArchive hypocenter to summary file */
		WriteHypoInverseArchive(pSumFileHypoInv[ngrid], hypo, Arrival, NumArrivals,
					fnout, 0, 1, gauss_par->arrivalWeightMax);
		/* also write to last.hypo_inv */
		sprintf(fname, "%slast.hypo_inv", f_outpath);
		if ((fp_tmp = fopen(fname, "w")) != NULL) {
			WriteHypoInverseArchive(fp_tmp, hypo, Arrival, NumArrivals,
					fnout, 0, 1, gauss_par->arrivalWeightMax);
			fclose(fp_tmp);
		}
	}

	if (iSaveHypoInvY2KArc) {
		/* write HypoInverseArchive hypocenter to summary file */
		WriteHypoInverseArchive(pSumFileHypoInvY2K[ngrid], hypo, Arrival, NumArrivals,
					fnout, 1, 1, gauss_par->arrivalWeightMax);
		/* also write to last.arc */
		sprintf(fname, "%slast.arc", f_outpath);
		if ((fp_tmp = fopen(fname, "w")) != NULL) {
			WriteHypoInverseArchive(fp_tmp, hypo, Arrival, NumArrivals,
					fnout, 1, 1, gauss_par->arrivalWeightMax);
			fclose(fp_tmp);
		}
	}

	if (iSaveAlberto4Sum) {
		/* write Alberto 4 SIMULPS format */
		WriteHypoAlberto4(pSumFileAlberto4[ngrid], hypo,
				  Arrival, fnout);
	}

	iWriteHypHeader[ngrid] = 0;

	return(0);

}




/** function to check and initialize an observation */

int checkObs(ArrivalDesc *arrival, int nobs)
{

	int istat;

	/* check for aliased arrival label */
	if ((istat = EvaluateArrivalAlias(arrival + nobs)) < 0)
		;
	/* set some fields */
	InitializeArrivalFields(arrival + nobs);
	/* check some fields */
	if (!isgraph(arrival[nobs].phase[0]))
		strcpy(arrival[nobs].phase, ARRIVAL_NULL_STR);
	if (!isgraph(arrival[nobs].comp[0]))
		strcpy(arrival[nobs].comp, ARRIVAL_NULL_STR);
	if (!isgraph(arrival[nobs].onset[0]))
		strcpy(arrival[nobs].onset, ARRIVAL_NULL_STR);
	if (!isgraph(arrival[nobs].first_mot[0]))
		strcpy(arrival[nobs].first_mot, ARRIVAL_NULL_STR);
	if (arrival[nobs].coda_dur < VERY_SMALL_DOUBLE)
		arrival[nobs].coda_dur = CODA_DUR_NULL;
	if (arrival[nobs].amplitude < VERY_SMALL_DOUBLE)
		arrival[nobs].amplitude = AMPLITUDE_NULL;
	if (arrival[nobs].period < VERY_SMALL_DOUBLE)
		arrival[nobs].period = PERIOD_NULL;
	if (message_flag >= 3) {
	// display arrival parameters
		sprintf(MsgStr,
			"Arrival %d:  %s (%s)  %s %s %s %d  %4.4d %2.2d %2.2d   %2.2d %2.2d %lf  Unc: %s %lf  Amp: %lf  Dur: %lf  Per: %lf",
			nobs,
			arrival[nobs].label,
			arrival[nobs].time_grid_label,
			arrival[nobs].onset,
			arrival[nobs].phase,
			arrival[nobs].first_mot,
			arrival[nobs].quality,
			arrival[nobs].year,
			arrival[nobs].month,
			arrival[nobs].day,
			arrival[nobs].hour,
			arrival[nobs].min,
			arrival[nobs].sec,
			arrival[nobs].error_type,
			arrival[nobs].error,
			arrival[nobs].amplitude,
			arrival[nobs].coda_dur,
			arrival[nobs].period);
		putmsg(3, MsgStr);
	}
	/* remove blanks/whitespace from phase string */
	removeSpace(arrival[nobs].phase);
	/* check for reject phase code */
	if (IsPhaseID(arrival[nobs].phase, "$")) {
		sprintf(MsgStr,
			"WARNING: phase code is $, rejecting observation: %s %s", arrival[nobs].label, arrival[nobs].phase);
		putmsg(2, MsgStr);
		return(-1);
	}
	/* check for valid P or S phase code */
/*INGV		if (!IsPhaseID(arrival[nobs].phase, "P")
	&& !IsPhaseID(arrival[nobs].phase, "S")) {
	sprintf(MsgStr,
	"WARNING: phase code not in P or S phase id list, rejecting observation: %s %s", arrival[nobs].label, arrival[nobs].phase);
	putmsg(2, MsgStr);
	return(-1);
}
	INGV*/
	/* check for duplicate arrival */
	if (nll_mode != MODE_DIFFERENTIAL) {
		// AJL 20041201 - alogoritm changed, less strict
		//if ( IsDuplicateArrival(arrival, nobs + 1, nobs, NULL) >= 0
		//		|| IsDuplicateArrival(arrival, nobs + 1, nobs, arrival[nobs].phase) >= 0 )
		//		) {
		if ( IsDuplicateArrival(arrival, nobs + 1, nobs, !iRejectDuplicateArrivals) >= 0 ) {
			sprintf(MsgStr,
				"WARNING: duplicate arrival, rejecting observation: %s %s", arrival[nobs].label, arrival[nobs].phase);
			putmsg(2, MsgStr);
			return(-1);
		}
	}

	// all OK
	return(1);

}



/** function read observation file and open station grid files */

int GetObservations(FILE* fp_obs, char* ftype_obs, char* fn_grids,
		    ArrivalDesc *arrival, int *pi_end_of_input,
		    int* pnignore, int* pnreject, int maxNumArrivals, HypoDesc* phypo,
		    int* pMaxArrExceeded, int *pnumSArrivals, int nobs_prev)
{

	int nobs, nobs_read, nobs_total;
	int istat, ntry, nLocate, n_NoAbsTime, n_compan, n_time_grid;
	char filename[FILENAME_MAX];
	char eval_phase[PHASE_LABEL_LEN];
	int isZeroWeight;
	char arrival_phase[PHASE_LABEL_LEN];
	int read_2d_sheets;
	int i_need_elev_corr, n_phs_try;

	SourceDesc* pstation;

	*pnumSArrivals = 0;

	/* initalize format specific event data */
	HypoInverseArchiveSumHdr[0] = '\0';


	/** read observations to arrival array */

	nobs = nobs_prev;
	*pnignore = 0;
	*pnreject = 0;
	nLocate = 0;
	ntry = 0;
	while ((istat = GetNextObs(fp_obs, arrival + nobs, ftype_obs, ntry++ == 0)) != EOF) {


		if (istat == OBS_FILE_FORMAT_ERROR) {
			putmsg(1, "ERROR: format error in observation file.");
			break;
		}
		if (istat == OBS_FILE_END_OF_EVENT) {
			if (nobs == 0)
				continue;
			else
				break;
		}
		if (istat == OBS_FILE_END_OF_INPUT) {
			*pi_end_of_input = 1;
			break;
		}
		if (istat == OBS_FILE_INVALID_PHASE)
			continue;
		if (istat == OBS_FILE_INVALID_DATE)
			continue;
		if (istat == OBS_FILE_SKIP_INPUT_LINE)
			continue;
		/* SH if comment line found -> read next obs */
		if (istat == OBS_IS_COMMENT_LINE)
			continue;
		if (nobs == MAX_NUM_ARRIVALS - 1) {
			if (!*pMaxArrExceeded) {
				*pMaxArrExceeded = 1;
				putmsg(1, "WARNING: maximum number of arrivals exceeded.");
			}
			continue;
		}

		// check and initialize observation
		if (checkObs(arrival, nobs) > 0)
			nobs++;
		// check and initialize second observation
		// SH if istat = OBS_FILE_TWO_ARRIVALS_READ then S-arrival has been read in -> two observations per line
		if (istat == OBS_FILE_TWO_ARRIVALS_READ)
			if (checkObs(arrival, nobs + 1) > 0)
				nobs++;

	}


	// save number of obs read
	nobs_total = nobs;
	nobs_read = nobs - nobs_prev;


	/** check for minimum number of arrivals */

//printf("*pi_end_of_input %d  nobs_read %d  MinNumArrLoc %d\n", *pi_end_of_input, nobs_read, MinNumArrLoc);

	if (*pi_end_of_input && nobs_total < 1) {
		return(nLocate + *pnignore);
	}

	if (!(*pi_end_of_input) && (nobs_total > 0 && nobs_total < MinNumArrLoc)) {
		sprintf(MsgStr,
			"WARNING: too few observations to locate, skipping event.");
		putmsg(1, MsgStr);
		if (message_flag >= 3) {
			sprintf(MsgStr,
				"INFO: %d observations needed (specified in control file entry LOCMETH).",
				MinNumArrLoc);
			putmsg(3, MsgStr);
		}
		return(-1);
	}



	/** process arrivals, determine if reject or ignore */

	// check for no absolute timing (inst begins with '*')
	n_NoAbsTime = CheckAbsoluteTiming(arrival + nobs_prev, nobs_read);

	/* homogenize date/time */
	if ((istat = HomogDateTime(arrival, nobs_total, phypo)) < 0) {
		puterr("ERROR: in arrival date/times.");
		if (istat == OBS_FILE_ARRIVALS_CROSS_YEAR_BOUNDARY)
			puterr("ERROR: arrivals cross year boundary.");
		return(-1);
	}


	/* sort to get arrivals in time order */
	if (nll_mode != MODE_DIFFERENTIAL && (istat = SortArrivalsTime(arrival, nobs_total)) < 0) {
		puterr("ERROR: sorting arrivals by time.");
		return(-1);
	}


	/* check each arrival and initialize */

	Num3DGridReadToMemory = 0;
	for (nobs = nobs_prev; nobs < nobs_total; nobs++) {

		if (message_flag >= 3) {
			sprintf(MsgStr, "Checking Arrival %d:  %s (%s)  %s %s %s %d",
				nobs,
				arrival[nobs].label,
				arrival[nobs].time_grid_label,
				arrival[nobs].onset,
				arrival[nobs].phase,
				arrival[nobs].first_mot,
				arrival[nobs].quality);
			putmsg(3, MsgStr);
		} else if (nobs_read > 1000 && message_flag > 0 && nobs % 100 == 0) {
			fprintf(stdout, "Checking Arrival %d/%d\r", nobs, nobs_read);
			fflush(stdout);
		}

		// make sure station location is null
		arrival[nobs].station.x = arrival[nobs].station.y = arrival[nobs].station.z = -LARGE_DOUBLE;


		// set some flags
		read_2d_sheets = 1;
		i_need_elev_corr = 0;

		// check for zero weight phase
		strcpy(arrival_phase, arrival[nobs].phase);
		isZeroWeight = 0;
		if (arrival_phase[0] == '*') {
			isZeroWeight = 1;
			strcpy(arrival_phase, arrival_phase + 1);
		}
		if (arrival[nobs].apriori_weight < VERY_SMALL_DOUBLE) {
			isZeroWeight = 1;
			/*printf("isZeroWeight Arrival %d:  %s (%s)  %s %s %s %d %lf",
			nobs,
			arrival[nobs].label,
			arrival[nobs].time_grid_label,
			arrival[nobs].onset,
			arrival[nobs].phase,
			arrival[nobs].first_mot,
			arrival[nobs].quality,
			arrival[nobs].apriori_weight);*/
		}

		strcpy(arrival[nobs].fileroot, "\0");
		arrival[nobs].n_companion = -1;
		arrival[nobs].n_time_grid = -1;
		arrival[nobs].tfact = 1.0;

		/* if Vp/Vs > 0, check for companion phase, its time grid will be used for times */
		if (VpVsRatio > 0.0) {
			/* try finding previously initialized companion phase */
			if (IsPhaseID(arrival_phase, "S") &&
						 (n_compan =
								 IsSameArrival(arrival, nobs, nobs, "P")) >= 0 &&
						 arrival[n_compan].flag_ignore == 0) {
				arrival[nobs].tfact = VpVsRatio;
				arrival[nobs].gdesc.type = arrival[n_compan].gdesc.type;
				arrival[nobs].station = arrival[n_compan].station;
				arrival[nobs].n_companion = n_compan;
				if (message_flag >= 3) {
					sprintf(MsgStr,
					"INFO: S phase: %d %s %s using companion phase %d travel time grids.",
					nobs, arrival[nobs].label, arrival[nobs].phase, arrival[nobs].n_companion);
					putmsg(3, MsgStr);
				}
				// save filename as grid identifier (needed for GridMemList)
				strcpy(arrival[nobs].gdesc.title, arrival[n_compan].gdesc.title);
						 }
		}

		/* if MODE_DIFFERENTIAL, check for same station phase, its time grid will be used for times */
		if (nll_mode == MODE_DIFFERENTIAL && arrival[nobs].n_companion < 0) {
			/* try finding previously initialized companion phase */
			if ((n_compan = IsSameArrival(arrival, nobs, nobs, NULL)) >= 0 &&
						  arrival[n_compan].flag_ignore == 0) {
				arrival[nobs].gdesc.type = arrival[n_compan].gdesc.type;
				arrival[nobs].station = arrival[n_compan].station;
				arrival[nobs].n_companion = n_compan;
				if (message_flag >= 3) {
					sprintf(MsgStr,
					"INFO: MODE_DIFFERENTIAL: %d %s %s using companion phase %d travel time grids.",
					nobs, arrival[nobs].label, arrival[nobs].phase, arrival[nobs].n_companion);
					putmsg(3, MsgStr);
				}
				// save filename as grid identifier (needed for GridMemList)
				strcpy(arrival[nobs].gdesc.title, arrival[n_compan].gdesc.title);
						  }
		}

		/* no previously initialized companion phase */
		if (arrival[nobs].n_companion < 0) {

			// try to open time grid file using original phase ID
			sprintf(arrival[nobs].fileroot, "%s.%s.%s", fn_grids,
				arrival_phase, arrival[nobs].time_grid_label);
			sprintf(filename, "%s.time", arrival[nobs].fileroot);
			// try opening time grid file for this phase
			istat = OpenGrid3dFile(filename,
					       &(arrival[nobs].fpgrid),
					       &(arrival[nobs].fphdr),
					       &(arrival[nobs].gdesc), "time",
					       &(arrival[nobs].station),
					       arrival[nobs].gdesc.iSwapBytes);

			if (istat < 0 ) {
				// try to open time grid file using LOCPHASEID mapped phase ID
				EvalPhaseID(eval_phase, arrival_phase);
				sprintf(arrival[nobs].fileroot, "%s.%s.%s", fn_grids,
					eval_phase, arrival[nobs].time_grid_label);
				sprintf(filename, "%s.time", arrival[nobs].fileroot);
				/* try opening time grid file for this phase */
				istat = OpenGrid3dFile(filename,
						&(arrival[nobs].fpgrid),
						&(arrival[nobs].fphdr),
						&(arrival[nobs].gdesc), "time",
						&(arrival[nobs].station),
						arrival[nobs].gdesc.iSwapBytes);
			}

			/* try opening P time grid file for S if no P companion phase */
			if (istat < 0 && VpVsRatio > 0.0 && IsPhaseID(arrival_phase, "S")) {
				arrival[nobs].tfact = VpVsRatio;
				sprintf(arrival[nobs].fileroot, "%s.%s.%s", fn_grids,
					"P", arrival[nobs].time_grid_label);
				sprintf(filename, "%s.time", arrival[nobs].fileroot);
				istat = OpenGrid3dFile(filename,
						&(arrival[nobs].fpgrid),
						&(arrival[nobs].fphdr),
						&(arrival[nobs].gdesc), "time",
						&(arrival[nobs].station),
						arrival[nobs].gdesc.iSwapBytes);
				if (message_flag >= 3) {
					sprintf(MsgStr,
							"INFO: S phase: using P phase travel time grid file: %s", filename);
					putmsg(3, MsgStr);
				}
			}

			// check if station/source in grid hdr file was DEFAULT
			if (istat >= 0 && strcmp(arrival[nobs].station.label, "DEFAULT") == 0) {
				// get station/source coordinates, etc
				pstation = FindSource(arrival[nobs].time_grid_label);
				if (pstation != NULL) {
					arrival[nobs].station = *pstation;
					i_need_elev_corr = 1;
				} else
					istat = -1;
			}

			// try opening DEFAULT time grid file
			if (istat < 0) {
				pstation = FindSource(arrival[nobs].time_grid_label);
				if (pstation != NULL) {
					// open DEFAULT time grid for this phase
					n_phs_try = 0;
					while (istat < 0 && n_phs_try < 2) {
						n_phs_try++;
						if (n_phs_try == 1)	// try to open time grid file using original phase ID
							strcpy(eval_phase, arrival_phase);
						else	// try to open time grid file using LOCPHASEID mapped phase ID
							EvalPhaseID(eval_phase, arrival_phase);
						sprintf(arrival[nobs].fileroot, "%s.%s.%s", fn_grids, eval_phase, "DEFAULT");
						sprintf(filename, "%s.time", arrival[nobs].fileroot);

						/* check if time grid already read */
						if ((n_time_grid = FindDuplicateTimeGrid(arrival, nobs, nobs)) >= 0 && arrival[n_time_grid].flag_ignore == 0) {
							arrival[nobs].gdesc.type = arrival[n_time_grid].gdesc.type;
							arrival[nobs].sheetdesc = arrival[n_time_grid].sheetdesc;
							arrival[nobs].station = *pstation;
							arrival[nobs].n_time_grid = n_time_grid;
							if (message_flag >= 3) {
								sprintf(MsgStr,
										"INFO: DEFAULT travel time: %d %s %s using previous phase %d travel time grids.",
										nobs, arrival[nobs].label, arrival[nobs].phase, arrival[nobs].n_time_grid);
								putmsg(3, MsgStr);
							}
							// save filename as grid identifier (needed for GridMemList)
							strcpy(arrival[nobs].gdesc.title, arrival[n_time_grid].gdesc.title);
							istat = 1;
						} else {

							istat = OpenGrid3dFile(filename,
									&(arrival[nobs].fpgrid),
									&(arrival[nobs].fphdr),
									&(arrival[nobs].gdesc), "time",
									&(arrival[nobs].station),
									arrival[nobs].gdesc.iSwapBytes);
							if (message_flag >= 3) {
								sprintf(MsgStr,
										"INFO: using DEFAULT travel time grid file: %s", filename);
								putmsg(3, MsgStr);
							}
							arrival[nobs].station = *pstation;
						}
					}
					i_need_elev_corr = 1;
					//read_2d_sheets = 0; // too slow!
				}
			}

			// save filename as grid identifier (needed for GridMemList)
			strcpy(arrival[nobs].gdesc.title, filename);

			if (istat < 0) {
				sprintf(MsgStr,
					"WARNING: cannot open time grid file: %s: rejecting observation: %s %s",
					filename, arrival[nobs].label, arrival[nobs].phase);
				putmsg(2, MsgStr);
				CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
				strcpy(arrival[nobs].fileroot, "\0");
				goto RejectArrival;
			}


			/* check that search grid is inside time grid (3D grids) */

			if (arrival[nobs].gdesc.type == GRID_TIME &&
						 !IsGridInside(LocGrid + 0, &(arrival[nobs].gdesc), 0)) {
				sprintf(MsgStr,
					"WARNING: initial location search grid not contained inside arrival time grid, rejecting observation: %s %s", arrival[nobs].label, arrival[nobs].phase);
				putmsg(1, MsgStr);
				CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
				goto RejectArrival;
						 }


			/* (2D grids) check that
						 (1) greatest distance from search grid to station
						 is inside time grid, and
						 (2) distance from center of search grid to station
						 is greater than DistStaGridMin (if DistStaGridMin > 0)
						 is less than DistStaGridMax (if DistStaGridMax > 0) */

						 if (arrival[nobs].gdesc.type == GRID_TIME_2D &&
										(istat = IsGrid2DBigEnough(LocGrid + 0,
								 &(arrival[nobs].gdesc),
								 &(arrival[nobs].station),
								 DistStaGridMin, DistStaGridMax,
								 LocGrid[0].origx + (LocGrid[0].dx
										* (double) (LocGrid[0].numx - 1)) / 2.0,
								 LocGrid[0].origy + (LocGrid[0].dy
										* (double) (LocGrid[0].numy - 1)) / 2.0)
										) != 1)
						 {
							 CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
							 if (istat == -1) {
								 sprintf(MsgStr,
										"WARNING: greatest distance from initial 3D location search grid to station \n\texceeds 2D time grid size, rejecting observation: %s %s",
										arrival[nobs].label, arrival[nobs].phase);
								 putmsg(2, MsgStr);
								 goto RejectArrival;
							 } else if (istat == -2) {
								 sprintf(MsgStr,
										"WARNING: distance from grid center to station \n\texceeds maximum station distance, ignoring observation in misfit calculation: %s %s",
										arrival[nobs].label, arrival[nobs].phase);
								 putmsg(2, MsgStr);
								 arrival[nobs].flag_ignore = 1;
								 goto IgnoreArrival;
							 }
							 if (istat == -3) {
								 sprintf(MsgStr,
										"WARNING: depth range of initial 3D location search grid exceeds that of 2D time grid size, rejecting observation: %s %s",
										arrival[nobs].label, arrival[nobs].phase);
								 putmsg(2, MsgStr);
								 goto RejectArrival;
							 }
						 }

		} /* if (arrival[nobs].n_companion < 0) */


		/* check for time delays */
		ApplyTimeDelays(arrival + nobs);
		;
		// calculate elevation correction if needed
		if (ApplyElevCorrFlag && i_need_elev_corr) {
			arrival[nobs].elev_corr = CalcSimpleElevCorr(arrival, nobs, ElevCorrVelP, ElevCorrVelS);
		}



		/* check if arrival is excluded */

		// zero weight or excluded in control file
		if (isZeroWeight || isExcluded(arrival[nobs].label, arrival[nobs].phase)) {
			sprintf(MsgStr,
				"INFO: arrival is excluded, ignoring observation in misfit calculation: %s %s",
				arrival[nobs].label, arrival[nobs].phase);
			arrival[nobs].flag_ignore = 1;
			CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
			putmsg(2, MsgStr);
			goto IgnoreArrival;
		}
		// no absolute time and not EDT
		if (!arrival[nobs].abs_time && LocMethod != METH_EDT) {
			sprintf(MsgStr,
				"INFO: arrival does not have absolute timing, ignoring observation in misfit calculation: %s %s %s",
				arrival[nobs].label, arrival[nobs].phase, arrival[nobs].inst);
			arrival[nobs].flag_ignore = 1;
			CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
			putmsg(2, MsgStr);
			goto IgnoreArrival;
		}
		// EDT_BOX but not BOX error
		if (!strcmp(arrival[nobs].error_type, "BOX") && LocMethod != METH_EDT_BOX) {
			sprintf(MsgStr,
				"INFO: method is EDT_BOX but arrival does not have error type BOX, ignoring observation in misfit calculation: %s %s %s",
				arrival[nobs].label, arrival[nobs].phase, arrival[nobs].inst);
			arrival[nobs].flag_ignore = 1;
			CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
			putmsg(2, MsgStr);
			goto IgnoreArrival;
		}


		/* check if maximum number of arrivals for location exceeded */

		if (nLocate + nobs_prev >= maxNumArrivals) {
			putmsg(2,
			       "WARNING: maximum number of arrivals for location exceeded, \n\tignoring observation in misfit calculation.");
			arrival[nobs].flag_ignore = 1;
			CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
			goto IgnoreArrival;
		}



		/** arrival to be used for location */

		/* no further processing for arrivals with companion time grids */
		if (arrival[nobs].n_companion >= 0 || arrival[nobs].n_time_grid >= 0)
			goto AcceptArrival;

		/** prepare time grids access in memory or on disk */

		/* construct dual-sheet description
		(2D grids for all search types,
		and 3D grids for grid-search) */

		if (SearchType == SEARCH_GRID
				  || (read_2d_sheets && arrival[nobs].gdesc.type == GRID_TIME_2D)) {

			arrival[nobs].sheetdesc = arrival[nobs].gdesc;
			//INGV ??
			//if (arrival[nobs].gdesc.numx > 1)
			arrival[nobs].sheetdesc.numx = 2;
			/* allocate grid */
			arrival[nobs].sheetdesc.buffer =
					AllocateGrid(&(arrival[nobs].sheetdesc));
			if (arrival[nobs].sheetdesc.buffer == NULL) {
				puterr(
						"ERROR: allocating memory for arrival sheet buffer.");
				goto RejectArrival;
				//return(EXIT_ERROR_MEMORY);
			}
			/* create array access pointers */
			arrival[nobs].sheetdesc.array =
					CreateGridArray(&(arrival[nobs].sheetdesc));
			if (arrival[nobs].sheetdesc.array == NULL) {
				puterr(
						"ERROR: creating array for accessing arrival sheet buffer.");
				goto RejectArrival;
				//return(EXIT_ERROR_MEMORY);
			}
			arrival[nobs].sheetdesc.origx = VERY_LARGE_DOUBLE;

		}


		/* read 3D grid into memory (3D grids for Metropolis or Octtree search) */

		if ((SearchType == SEARCH_MET || SearchType == SEARCH_OCTTREE)
				   && arrival[nobs].gdesc.type == GRID_TIME
				   && (MaxNum3DGridMemory < 0 ||
				   Num3DGridReadToMemory < MaxNum3DGridMemory)) {

			/* allocate grid */
			arrival[nobs].gdesc.buffer =
					NLL_AllocateGrid(&(arrival[nobs].gdesc));
			if (arrival[nobs].gdesc.buffer == NULL) {
				puterr(
						"ERROR: allocating memory for arrival time grid buffer; grid will be read from disk.");
			} else {
				/* create array access pointers */
				arrival[nobs].gdesc.array =
						NLL_CreateGridArray(&(arrival[nobs].gdesc));
				if (arrival[nobs].gdesc.array == NULL) {
					puterr(
							"ERROR: creating array for accessing arrival time grid buffer.");
					goto RejectArrival;
					//return(EXIT_ERROR_MEMORY);
				}
				/* read time grid */
				if (NLL_ReadGrid3dBuf(&(arrival[nobs].gdesc),
				    arrival[nobs].fpgrid) < 0) {
					    puterr(
							    "ERROR: reading arrival time grid buffer.");
					    goto RejectArrival;
					//return(EXIT_ERROR_MEMORY);
				    }
				    CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
				    Num3DGridReadToMemory++;
			}
		}


		/* read time grid and close file (2D grids)*/

		if (read_2d_sheets && arrival[nobs].gdesc.type == GRID_TIME_2D) {
			istat = ReadArrivalSheets(1, &(arrival[nobs]), 0.0);
			CloseGrid3dFile(&(arrival[nobs].fpgrid), &(arrival[nobs].fphdr));
			if (istat  < 0) {
				sprintf(MsgStr,
					"ERROR: reading arrival travel time sheets (2D grid), rejecting observation: %s %s",
					arrival[nobs].label, arrival[nobs].phase);
				puterr(MsgStr);
				goto RejectArrival;
			}
		}

		/* arrival accepted, use for location */
		AcceptArrival:
				arrival[nobs].flag_ignore = 0;
		nLocate++;
		if (IsPhaseID(arrival[nobs].phase, "S"))
			(*pnumSArrivals)++;


		/* arrival accepted, ignore for location */
		IgnoreArrival:

				*pnignore += arrival[nobs].flag_ignore;

		continue;


		/* arrival rejected */
		RejectArrival:

				(*pnreject)++;
		arrival[nobs].flag_ignore = 999;
		if (message_flag >= 3) {
			sprintf(MsgStr, "   Rejected Arrival %d:  %s (%s)  %s %s %s %d",
				nobs,
				arrival[nobs].label,
				arrival[nobs].time_grid_label,
				arrival[nobs].onset,
				arrival[nobs].phase,
				arrival[nobs].first_mot,
				arrival[nobs].quality);
			putmsg(3, MsgStr);
		}

	}


	/* avoid returning 0 if arrivals were read, return 0 indicates end of file */
	if (nLocate + *pnignore == 0 && nobs_read > 0) {
		sprintf(MsgStr,
			"WARNING: %d arrivals read, but none accepted for location.", nobs_read);
		putmsg(2, MsgStr);
		return(-1);
	}

	return(nLocate + *pnignore);
}



/** function to initialize arrival fields */

void InitializeArrivalFields(ArrivalDesc *arrival)
{

	arrival->abs_time = 1;
	arrival->obs_centered = 0.0;
	arrival->pred_travel_time = 0.0;
	arrival->pred_travel_time_best = -LARGE_DOUBLE;
	arrival->pred_centered = 0.0;
	arrival->cent_resid = 0.0;
	arrival->obs_travel_time = 0.0;
// 	arrival->delay = 0.0;	// do not initialize here!
	arrival->elev_corr = 0.0;
	arrival->residual = 0.0;
	arrival->dist = 0.0;
	arrival->azim = 0.0;
	arrival->ray_azim = 0.0;
	arrival->ray_dip = 0.0;
	arrival->ray_qual = 0;
	arrival->pdf_residual_sum = 0.0;
	arrival->pdf_weight_sum = 0.0;

	arrival->fpgrid = NULL;
	arrival->fphdr = NULL;
	arrival->gdesc.buffer = NULL;
	arrival->gdesc.iSwapBytes = iSwapBytesOnInput;
	arrival->sheetdesc.buffer = NULL;

	arrival->station_weight = 1.0;


//DD
	if (nll_mode != MODE_DIFFERENTIAL) {
		arrival->weight = 0.0;
	}


}


/** function to test if arrival is excluded */

int isExcluded(char *label, char *phase) {

	int nexclude;

	for (nexclude = 0; nexclude < NumLocExclude; nexclude++) {
		if (strcmp(label, LocExclude[nexclude].label) == 0
				  && strcmp(phase, LocExclude[nexclude].phase) == 0)
			return(1);
	}

	return(0);

}



/** function to determine type P or S of last leg of phase */

// returns 'P' if P, 'S' if S, ' ' otherwise

char lastLegType(ArrivalDesc *arrival)
{
	char *c_last_p, *c_last_P, *c_last_s, *c_last_S;
	int i_last_p, i_last_P, i_last_s, i_last_S;


	// get offsets to last char of types p/P or s/S
	c_last_p = strrchr(arrival->phase, 'p');
	i_last_p = c_last_p == NULL ? -1 : (int) (c_last_p - arrival->phase);
	c_last_P = strrchr(arrival->phase, 'P');
	i_last_P = c_last_P == NULL ? -1 : (int) (c_last_P - arrival->phase);
	c_last_s = strrchr(arrival->phase, 's');
	i_last_s = c_last_s == NULL ? -1 : (int) (c_last_s - arrival->phase);
	c_last_S = strrchr(arrival->phase, 'S');
	i_last_S = c_last_S == NULL ? -1 : (int) (c_last_S - arrival->phase);

	// determine type of last char
	i_last_p = i_last_p > i_last_P ? i_last_p : i_last_P;
	i_last_s = i_last_s > i_last_S ? i_last_s : i_last_S;
	if (i_last_p >= 0 && i_last_p > i_last_s)
		return('P');
	if (i_last_s >= 0 && i_last_s > i_last_p)
		return('S');
	return(' ');

}



/** function to calculuate simple elevation correction based on surface velcoity */

double CalcSimpleElevCorr(ArrivalDesc *arrival, int narr, double pvel, double svel)
{
	double yval_grid, t_surface, t_elev, elev_corr;
	int n_compan, diagnostic;


	// Switch debug messages from this function on/off (1/0).
	diagnostic = message_flag >= 3;
//diagnostic = 1;

	// check for companion
	if ((n_compan = arrival[narr].n_companion) >= 0) {
		if (diagnostic) {
			sprintf(MsgStr,"CalcSimpleElevCorr: n_compan=%d", n_compan);
			putmsg(1, MsgStr);
		}
		if ((elev_corr = arrival[n_compan].elev_corr) < 0.0)
			return(0.0);
	// check for specific P or S vel
	} else if (pvel > 0.0 && lastLegType(arrival + narr) == 'P') {
		elev_corr = -arrival[narr].station.depth / pvel;
	} else if (svel > 0.0 && lastLegType(arrival + narr) == 'S') {
		elev_corr = -arrival[narr].station.depth / svel;
	// else check grid type
	} else {
		if (arrival[narr].gdesc.type == GRID_TIME) {
			if (diagnostic) {
				sprintf(MsgStr,"CalcSimpleElevCorr: GRID_TIME");
				putmsg(1, MsgStr);
			}
			/* 3D grid */
			if ((t_surface = (double) ReadAbsInterpGrid3d(arrival[narr].fpgrid,
			     &(arrival[narr].gdesc), 0.0, 0.0, 0.0)) < 0.0)
				return(0.0);
			// read pos along X dir because grid may not extend above surface
			if ((t_elev = (double) ReadAbsInterpGrid3d(arrival[narr].fpgrid,
			     &(arrival[narr].gdesc), fabs(arrival[narr].station.depth), 0.0, 0.0)) < 0.0)
				return(0.0);
		} else {
			if (diagnostic) {
				sprintf(MsgStr,"CalcSimpleElevCorr: GRID_TIME_2D");
				putmsg(1, MsgStr);
			}
			/* 2D grid (1D model) */
			if ((t_surface = ReadAbsInterpGrid2d(arrival[narr].fpgrid,
			     &(arrival[narr].gdesc), 0.0, 0.0)) < 0.0)
				return(0.0);
			// read pos along Horiz dir because grid may not extend above surface
			yval_grid = fabs(arrival[narr].station.depth);
			if (GeometryMode == MODE_GLOBAL)
				yval_grid *= KM2DEG;
			if ((t_elev = ReadAbsInterpGrid2d(arrival[narr].fpgrid,
			     &(arrival[narr].gdesc), yval_grid, 0.0)) < 0.0)
				return(0.0);
		}
		if (arrival[narr].station.depth > 0.0)	// below surface
			t_elev *= -1.0;
		elev_corr = t_elev - t_surface;
	}

	elev_corr *= arrival[narr].tfact;

	if (diagnostic) {
		sprintf(MsgStr,"CalcSimpleElevCorr: lat=%.3f  lon=%.3f  depth=%.3f  elev_corr=%.3f",
			arrival[narr].station.dlat, arrival[narr].station.dlong, arrival[narr].station.depth, elev_corr);
		putmsg(1, MsgStr);
	}

	return (elev_corr);

}



/** function to evaluate arrival label aliases */

int EvaluateArrivalAlias(ArrivalDesc *arrival)
{
	int nAlias;
	int checkAgain = 1, icount = 0, aliasApplied = 0;
	char *pchr, tmpLabel[MAXLINE];


	strcpy(tmpLabel, arrival->label);

	if (message_flag >= 4) {
		sprintf(MsgStr, "Checking for station name alias: %s", tmpLabel);
		putmsg(4, MsgStr);
	}


	/* evaluate aliases until no replacement done */

	while(checkAgain && icount < MAX_NUM_LOC_ALIAS_CHECKS) {

		checkAgain = 0;
		icount++;

		for (nAlias = 0; nAlias < NumLocAlias; nAlias++) {

			/* check if alias can be rejected */

			if (strcmp(LocAlias[nAlias].name, tmpLabel) != 0)
				continue;
			if (LocAlias[nAlias].byr > arrival->year)
				continue;
			if (LocAlias[nAlias].byr == arrival->year) {
				if (LocAlias[nAlias].bmo > arrival->month)
					continue;
				if (LocAlias[nAlias].bmo == arrival->month
								&& LocAlias[nAlias].bday > arrival->day)
					continue;
			}
			if (LocAlias[nAlias].eyr < arrival->year)
				continue;
			if (LocAlias[nAlias].eyr == arrival->year) {
				if (LocAlias[nAlias].emo < arrival->month)
					continue;
				if (LocAlias[nAlias].emo == arrival->month
								&& LocAlias[nAlias].eday < arrival->day)
					continue;
			}

			/* apply alias */

			aliasApplied = 1;

			strcpy(tmpLabel, LocAlias[nAlias].alias);
			if (message_flag >= 3) {
				sprintf(MsgStr, " -> %s", tmpLabel);
				putmsg(4, MsgStr);
			}

			/* if alias label is same as arrival label,
			end check to avoid infinite recurssion */
			if (strcmp(tmpLabel, arrival->label) == 0)
				checkAgain = 0;
			else
				checkAgain = 1;

			break;

		}
	}


	/* check for possible recursion in alias specifications */

	if (icount >= MAX_NUM_LOC_ALIAS_CHECKS) {
		if (message_flag >= 4)
			putmsg(4, "");
		puterr("ERROR: possible infinite recursion in station name alias.");
		return(-1);
	}


	/* update arrival label */

	strcpy(arrival->time_grid_label, tmpLabel);
	if ((pchr = strrchr(tmpLabel, '_')) != NULL)
		*pchr = '\0';
//18JUL2002 AJL (IRSN)
// to prevent station label change in output file
//	strcpy(arrival->label, tmpLabel);


	/* return if no alias applied */

	if (!aliasApplied) {
		if (message_flag >= 4)
			putmsg(4, "");
		return(0);
	}

	if (message_flag >= 4)
		putmsg(4, "");


	return(0);
}



/** function to apply time delays */

int ApplyTimeDelays(ArrivalDesc *arrival)
{
	int nDelay, i;
	int ifound;
	double tmp_delay;

	double tmp_std_dev;

	/*
	// added SH 23.10.2007
	char eval_phase[PHASE_LABEL_LEN];
	char arrival_phase[PHASE_LABEL_LEN];

	// SH 23.10.2007 do phase mapping before checking for time delays
	strcpy(arrival_phase, arrival->phase);
	EvalPhaseID(eval_phase, arrival_phase);

	sprintf(MsgStr, "Checking for time delay: %s %s",
		arrival->label, eval_phase);
	putmsg(4, MsgStr);
	*/

	if (message_flag >= 4) {
		sprintf(MsgStr, "Checking for time delay: %s %s",
			arrival->label, arrival->phase);
		putmsg(4, MsgStr);
	}

	arrival->delay = 0.0;

	/* check time delays for match to label/phase */

	ifound = 0;
	for (nDelay = 0; !ifound && nDelay < NumTimeDelays; nDelay++) {

		if (strcmp(TimeDelay[nDelay].label, arrival->label) == 0
				  && strcmp(TimeDelay[nDelay].phase, arrival->phase) == 0)
		{
			/* apply station delays */

			tmp_delay = TimeDelay[nDelay].delay;
			arrival->delay = 0.0;
			if (fabs(tmp_delay) > VERY_SMALL_DOUBLE) {
// DELAY_CORR			arrival->sec -= tmp_delay;
				arrival->delay = tmp_delay;
				arrival->obs_time -= (long double) arrival->delay;   // DELAY_CORR	- incorporating delay so subtract (Tcorr = Tobs - (O-C))
				if (message_flag >= 4) {
					sprintf(MsgStr,
							"   delay of %lf sec subtracted from obs time.",
							tmp_delay);
					putmsg(4, MsgStr);
				}
				ifound = 1;
			// adjust error based on std-dev of delays
			// AJL 20030826 added to test if pick error should be set based on residual statistics
				if (0) {
					tmp_std_dev = TimeDelay[nDelay].std_dev;
					if (tmp_std_dev > arrival->error) {
						if (message_flag >= 4) {
							sprintf(MsgStr, "   error set from %f to %f sec.", arrival->error, tmp_std_dev);
							putmsg(4, MsgStr);
						}
						arrival->error = tmp_std_dev;
					} else {
						if (message_flag >= 4) {
							sprintf(MsgStr, "   error not changed from %f to %f sec.", arrival->error, tmp_std_dev);
							putmsg(4, MsgStr);
						}
					}
				}
			}
			break;
		}
	}
	if (message_flag >= 4)
		putmsg(4, "");


	// if time delay not found, check for delay surface
	if (!ifound && NumTimeDelaySurface) {
		tmp_delay = LARGE_FLOAT;
		for (i = 0; i < NumTimeDelaySurface; i++) {
			if (strcmp(arrival->phase, TimeDelaySurfacePhase[i]) == 0) {
				tmp_delay = ApplySurfaceTimeDelay(i, arrival);
				tmp_delay *= TimeDelaySurfaceMultiplier[i];
				break;
			}
		}
		if (i < NumTimeDelaySurface && tmp_delay < LARGE_FLOAT / 2.0) {
			arrival->delay = tmp_delay;
			arrival->obs_time -= (long double) arrival->delay;   // DELAY_CORR	- incorporating delay so subtract (Tcorr = Tobs - (O-C))
			printf("%s %s %s, ", arrival->label, arrival->phase, TimeDelaySurfacePhase[i]);
			if (message_flag >= 1) {
				sprintf(MsgStr,
					"    %s surface delay of %lf sec at lat %f, long %f subtracted from obs time.",
					TimeDelaySurfacePhase[i], tmp_delay,
					arrival->station.dlat, arrival->station.dlong);
				//putmsg(4, MsgStr);
				putmsg(1, MsgStr);
			}
		}
	}


	return(0);
}




/** function to get time delay from time delay surface */

double ApplySurfaceTimeDelay(int nsurface, ArrivalDesc *arrival)
{
	double x, y;

	if (arrival->station.is_coord_latlon) {
		x = arrival->station.dlong;
		y = arrival->station.dlat;
	} else {
		return(LARGE_FLOAT);
	}

	return(get_surface_z(nsurface, x, y));
}




/** function to extract arrival information observation file name */

/* returns: 0 if nothing done, 1 if info extracted, -1 if unexpected error */

int ExtractFilenameInfo(char *filename, char *type_obs)
{
	int istat;
	char *filepos, *extpos;

	if (strcmp(ftype_obs, "RENASS_DEP") == 0) {
		/* find beginning of filename */
		if ((filepos = strrchr(filename, '/')) == NULL)
			return(-1);
		/* get date/time from filename */
		/* try long format (i.e. NICE199801311202.dep) */
		if ((extpos = strstr(filepos, ".dep")) != NULL &&
				   extpos - filepos - 12 >= 0)
		{
			if ((istat = sscanf(extpos - 12, "%4d%2d%2d%2d%2d",
			     &EventTime.year, &EventTime.month, &EventTime.day,
			     &EventTime.hour, &EventTime.min))
						  != 5)
				return(-1);
		}
		/* try short format (i.e. g504210802.dep) */
		else if ((extpos = strstr(filepos, ".dep")) != NULL &&
					extpos - filepos - 9 >= 0)
		{
			if ((istat = sscanf(extpos - 9, "%1d%2d%2d%2d%2d",
			     &EventTime.year, &EventTime.month, &EventTime.day,
			     &EventTime.hour, &EventTime.min))
						  != 5)
				return(-1);
			EventTime.year += 1990;
		}

		return(1);
	}

	return(0);

}


/** function to read arrival from observation file */

int GetNextObs(FILE* fp_obs, ArrivalDesc *arrival, char* ftype_obs, int nfirst)
{
	int istat = 0, iloop;
	char *cstat;
	char chr, instruction[MAXSTRING];
	char chrtmp[MAXSTRING];

	double psec, ssec;

	double weight, ttime;

	int ioff;

	static char line[MAXLINE_LONG];

	static int date_saved, year_save, month_save, day_save;
	static int check_for_S_arrival;

	static int in_hypocenter_event;

	int ifound;

	// ETH LOC format
	char eth_line_key[MAXSTRING];
	int eth_use_loc = 0, eth_use_mag = 0;
	double  vpvs;

	// NEIC / ISC format
	static char last_label[10];
	char cmonth[4];
	char* pchr;
	int origin_hour;

	// ISC format
	char isc_time_str[10];

	// DD
	static int hypo_cc_flag;
	static long int dd_event_id_1, dd_event_id_2;
	static double dd_otime_corr;
	double tt_sta1, tt_sta2;

	// HYPOINVERSE_Y2000_ARC
	int idummy;

	// SAFOD
	int yearday;
	double left_uncertainty, right_uncertainty;


	/* if no obs read for this event, set date saved flag to 0 */
	if (nfirst) {
		date_saved = 0;
		check_for_S_arrival = 0;
		in_hypocenter_event = 0;
	}


	/* check for special control instructions */
	iloop = 1;
	while (iloop && !check_for_S_arrival) {
		iloop = 0;
		if ((chr = fgetc(fp_obs)) == '!'/* || chr == '#'*/) {
			ungetc(chr, fp_obs);
			fgets(line, MAXLINE_LONG, fp_obs);
			printf("1 %s", line);
			/* read instruction */
			istat = sscanf(line + 1, "%s", instruction);
			if (istat == EOF)
				return(OBS_FILE_END_OF_INPUT);
			if (istat == 1) {
				if (strcmp(instruction, "END_EVENT") == 0) {
					return(OBS_FILE_END_OF_EVENT);
				} else if (strcmp(instruction, "END_FILE") == 0) {
					return(OBS_FILE_END_OF_INPUT);
				} else if (strcmp(instruction, "SKIP_NEXT_LINE") == 0) {
					fgets(line, MAXLINE_LONG, fp_obs);
					printf("2 %s", line);

					iloop = 1;
					continue;
				} else {		// skip this line
					iloop = 1;
					continue;
				}
			}
			if (istat != 1) {
				puterr2("WARNING: unrecognized control statement", line);
			}
		} else {
			ungetc(chr, fp_obs);
		}
	}


	/* set field defaults */
	strcpy(arrival->label,  ARRIVAL_NULL_STR);
	strcpy(arrival->network,  ARRIVAL_NULL_STR);
	strcpy(arrival->inst,  ARRIVAL_NULL_STR);
	strcpy(arrival->comp,  ARRIVAL_NULL_STR);
	strcpy(arrival->onset,  ARRIVAL_NULL_STR);
	strcpy(arrival->phase,  ARRIVAL_NULL_STR);
	strcpy(arrival->first_mot,  ARRIVAL_NULL_STR);
	arrival->quality = 99;
	strcpy(arrival->error_type, "GAU");
	arrival->error = 0.0;
	arrival->coda_dur = CODA_DUR_NULL;
	arrival->amplitude = AMPLITUDE_NULL;
	arrival->period = PERIOD_NULL;
	arrival->amp_mag = MAGNITUDE_NULL;
	arrival->dur_mag = MAGNITUDE_NULL;
	arrival->apriori_weight = 1.0;


	/* attempt to read obs based on obs file type */

	if (strcmp(ftype_obs, "NLLOC_OBS") == 0)
	{

		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		istat = ReadArrival(line, arrival, IO_ARRIVAL_OBS);

		if (istat < 1)
			return(OBS_FILE_END_OF_EVENT);

		/* convert error to quality */
		if ((arrival->quality = Err2Qual(arrival)) < 0)
			arrival->quality = 99;
		return(istat);
	}


/* SH Seismograph Station University of Utah format (PING)
	first line: reftime (yymmddhhmm)
	next lines arrival times rel. to reftime incl. duration
	*/

	else if (strcmp(ftype_obs,"UUSS") == 0)
	{
		/* interpret line: reftime or phase information or comment line etc */
		chr = fgetc(fp_obs);
		/* fprintf(stderr,"GetNextObs: chr %d\n",chr); */
		if (chr == EOF) {
			return(OBS_FILE_END_OF_INPUT);
		} else if (chr == 99 || chr == 67) {
			/* comment line starts with 'c' */
			/* read until end of line */
			ungetc(chr,fp_obs);
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			return(OBS_IS_COMMENT_LINE);
		} else if (chr == 35) {
			/* end of event marked by '#' */
			/* read until end of line */
			ungetc(chr,fp_obs);
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			return(OBS_FILE_END_OF_EVENT);
		} else if (chr == 32) {
			/* line contains reference time starting with a space */
			ungetc(chr,fp_obs);
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			istat = ReadFortranInt(line,2,2,&EventTime.year);
			if (EventTime.year < 20)
				EventTime.year += 100;
			EventTime.year += 1900;
			istat += ReadFortranInt(line,4,2,&EventTime.month);
			istat += ReadFortranInt(line,6,2,&EventTime.day);
			istat += ReadFortranInt(line,8,2,&EventTime.hour);
			istat += ReadFortranInt(line,10,2,&EventTime.min);
			arrival->sec = 0.0;
		} else {
			/* line contains phase information */
			ungetc(chr,fp_obs);
		}

		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* now read phase info */
		istat = ReadFortranString(line,1,4,arrival->label);
		/* check if comment line */
		if ( (strncmp(arrival->label,"c",1) == 0) || (strncmp(arrival->label,"C",1) == 0) ) {
			return(OBS_IS_COMMENT_LINE);
		}
		TrimString(arrival->label);
		strcpy(arrival->phase,"P");
		if ( (istat += ReadFortranString(line,6,1,arrival->first_mot)) != 2) {
			/* read next lines until arrival time is found */
			while ((cstat = fgets(line, MAXLINE_LONG, fp_obs)) != NULL) {
				if (strncmp(line,"#",1) == 0 )
					/* end of event detected */
					return (OBS_FILE_END_OF_EVENT);
				istat = 0;
				/* fprintf(stderr,"GetNextObs: %s\n",line); */
				/* fprintf(stderr,"GetNextObs: %d %s\n",istat,arrival->first_mot); */
				if ( (istat += ReadFortranString(line,6,1,arrival->first_mot)) == 1 ) {
					istat = ReadFortranString(line,1,4,arrival->label);
					if ( (strncmp(arrival->label,"c",1) == 0) || (strncmp(arrival->label,"C",1) == 0) ){
						return(OBS_IS_COMMENT_LINE);
					}
					TrimString(arrival->label);
					break;
				}
			}
		}

		if ( cstat == NULL )
			return (OBS_FILE_END_OF_INPUT);

		/* continue with reading of remaining phase information */
		istat += ReadFortranInt(line,10,1,&arrival->quality);
		istat += ReadFortranReal(line,12,6,&arrival->sec);

		arrival->year = EventTime.year;
		arrival->month = EventTime.month;
		arrival->day = EventTime.day;
		arrival->hour = EventTime.hour;
		arrival->min = EventTime.min;

		/* convert quality to error */
		Qual2Err(arrival);

		/* fprintf(stderr,"GetNextObs: phase %4s %1s %1s %1d %5.2f\n",arrival->label,
		arrival->phase,arrival->first_mot,arrival->quality,arrival->sec); */

		/* check for possible S-arrival */
		istat += ReadFortranReal(line, 21, 6, &ssec);
		if ( ssec > 0.0) {
			arrival++;
			istat = ReadFortranString(line,1,4,arrival->label);
			TrimString(arrival->label);
			strcpy(arrival->phase,"S");
			istat += ReadFortranInt(line,19,1,&arrival->quality);
			arrival->year = EventTime.year;
			arrival->month = EventTime.month;
			arrival->day = EventTime.day;
			arrival->hour = EventTime.hour;
			arrival->min = EventTime.min;
			arrival->sec = ssec;
			/* convert quality to error */
			Qual2Err(arrival);
			return(OBS_FILE_TWO_ARRIVALS_READ);
		} else
			return(istat);
	}


	else if (strcmp(ftype_obs, "HYPO71") == 0 ||
			strcmp(ftype_obs, "HYPO71_OV") == 0 ||
			strcmp(ftype_obs, "HYPOELLIPSE") == 0)
	{

		if (check_for_S_arrival) {
			/* check for S phase input in last input line read */

			/* set S read offset to allow correction of incorrect hypo71 format */
			ioff = 0;
			if (strcmp(ftype_obs, "HYPO71_OV") == 0) {
				istat = ReadFortranString(line, 31, 1, chrtmp);
				if  (strcmp(chrtmp, " ") != 0)
					ioff = -1;
			}

			/* check for zero or blank S phase time */
			istat = ReadFortranString(line, 32 + ioff, 5, chrtmp);
			if (istat > 0
						 && strncmp(chrtmp, "     ", 5) != 0
						 && strncmp(chrtmp, "    0", 5) != 0)
			{
				/* read S phase input in last input line read */
				istat = ReadFortranString(line, 1, 4, arrival->label);
				TrimString(arrival->label);
				istat += ReadFortranInt(line, 10, 2, &arrival->year);
		// temporary fix for HYPO71 Y2K problem
				if (arrival->year < 20)
					arrival->year += 100;
				arrival->year += 1900;
				istat += ReadFortranInt(line, 12, 2, &arrival->month);
				istat += ReadFortranInt(line, 14, 2, &arrival->day);
				istat += ReadFortranInt(line, 16, 2, &arrival->hour);
				istat += ReadFortranInt(line, 18, 2, &arrival->min);
				istat += ReadFortranReal(line, 32 + ioff, 5, &arrival->sec);
				/* check for integer sec format */
				if (arrival->sec > 99.999)
					arrival->sec /= 100.0;
				istat += ReadFortranReal(line, 20, 5, &psec);
				/* check for P second >= 60.0 */
				if (psec >= 60.0 && arrival->sec < 60.0)
					arrival->sec += 60.0;
				arrival->phase[0] = '\0';
				istat += ReadFortranString(line, 38 + ioff, 1, arrival->phase);
				TrimString(arrival->phase);
				istat += ReadFortranInt(line, 40 + ioff, 1, &arrival->quality);
			}
		}

		/* check for S arrival input found */
		if (check_for_S_arrival && istat == 10
				  && IsPhaseID(arrival->phase, "S")
				  && IsGoodDate(arrival->year,
						  arrival->month, arrival->day)) {

			//strcpy(arrival->phase, "S");

							  /* set error fields */
							  strcpy(arrival->error_type, "GAU");
							  if (arrival->quality >= 0 &&
										arrival->quality < NumQuality2ErrorLevels) {
								  arrival->error =
										Quality2Error[arrival->quality];
										} else {
										arrival->error =
										Quality2Error[NumQuality2ErrorLevels - 1];
										puterr("WARNING: invalid arrival weight.");
										}

										line[0] = '\0';
										check_for_S_arrival = 0;
										return(istat);
						  } else if (check_for_S_arrival) {
							  check_for_S_arrival = 0;
							  return(OBS_FILE_SKIP_INPUT_LINE);
						  }




						  /* read next line */
						  cstat = fgets(line, MAXLINE_LONG, fp_obs);
						  if (cstat == NULL)
							  return(OBS_FILE_END_OF_INPUT);

						  /* read formatted P (or S) arrival input */
						  istat = ReadFortranString(line, 1, 4, arrival->label);
						  TrimString(arrival->label);
						  istat += ReadFortranString(line, 5, 1, arrival->onset);
						  TrimString(arrival->onset);
						  istat += ReadFortranString(line, 6, 1, arrival->phase);
						  TrimString(arrival->phase);
						  istat += ReadFortranString(line, 7, 1, arrival->first_mot);
						  TrimString(arrival->first_mot);
						  istat += ReadFortranInt(line, 8, 1, &arrival->quality);
						  istat += ReadFortranInt(line, 10, 2, &arrival->year);
// temporary fix for HYPO71 Y2K problem
						  if (arrival->year < 20)
							  arrival->year += 100;
						  arrival->year += 1900;
						  istat += ReadFortranInt(line, 12, 2, &arrival->month);
						  istat += ReadFortranInt(line, 14, 2, &arrival->day);
						  istat += ReadFortranInt(line, 16, 2, &arrival->hour);
						  istat += ReadFortranInt(line, 18, 2, &arrival->min);
						  istat += ReadFortranReal(line, 20, 5, &arrival->sec);

						  if (istat != 11) {
							  line[0] = '\0';
							  return(OBS_FILE_END_OF_EVENT);
						  }

						  /* read optional amplitude/period fields */
						  istat += ReadFortranReal(line, 44, 4, &arrival->amplitude);
//printf("AMPLITUDE: %lf (%s)\n", arrival->amplitude, line);
						  istat += ReadFortranReal(line, 48, 3, &arrival->period);
// 20030826 AJL coda_dur added
						  istat += ReadFortranReal(line, 71, 5, &arrival->coda_dur);


						  check_for_S_arrival = 1;
						  /* check for valid phase code */
//		if (IsPhaseID(arrival->phase, "P")) {
			//strcpy(arrival->phase, "P");
						  check_for_S_arrival = 1;

//		} else if (IsPhaseID(arrival->phase, "S")) {
			//strcpy(arrival->phase, "S");
//			check_for_S_arrival = 0;
//		} else
//			return(OBS_FILE_END_OF_EVENT);

						  if (!IsGoodDate(arrival->year, arrival->month, arrival->day))
							  return(OBS_FILE_END_OF_EVENT);


						  /* check for integer sec format */
						  if (arrival->sec > 99.999)
							  arrival->sec /= 100.0;

						  /* convert quality to error */
						  Qual2Err(arrival);

						  return(istat);
	}



	else if (strcmp(ftype_obs, "HYPOINVERSE_Y2000_ARC") == 0)
	{

/*
		Y2000 (station) archive format
		StartCol. Len. FortranFormat Data (* revised from pre-Y2000 format)
		1 5 A5 5-letter station site code, left justified. *
		6 2 A2 2-letter seismic network code. *
		8 1 1X Blank *
		9 1 A1 One letter station component code.
		10 3 A3 3-letter station component code. *
		13 1 1X Blank *
		14 2 A2 P remark such as "IP".
		16 1 A1 P first motion.
		17 1 I1 Assigned P weight code.
		18 4 I4 Year. *
		22 8 4I2 Month, day, hour and minute.
		30 5 F5.2 Second of P arrival.
		35 4 F4.2 P travel time residual.
		39 3 F3.2 P weight actually used.
		42 5 F5.2 Second of S arrival.
		47 2 A2 S remark such as "ES".
		49 1 1X Blank
		50 1 I1 Assigned S weight code.
		51 4 F4.2 S travel time residual.
		55 7 F7.2 Amplitude (Normally peak-to-peak). *
		62 2 I2 Amp units code. 0=PP mm, 1=0 to peak mm (UCB), 2=digital counts. *
		64 3 F3.2 S weight actually used.
		67 4 F4.2 P delay time.
		71 4 F4.2 S delay time.
		75 4 F4.1 Epicentral distance (km).
		79 3 F3.0 Emergence angle at source.
		82 1 I1 Amplitude magnitude weight code.
		83 1 I1 Duration magnitude weight code.
		84 3 F3.2 Period at which the amplitude was measured for this station.
		87 1 A1 1-letter station remark.
		88 4 F4.0 Coda duration in seconds.
		92 3 F3.0 Azimuth to station in degrees E of N.
		95 3 F3.2 Duration magnitude for this station. *
		98 3 F3.2 Amplitude magnitude for this station. *
		101 4 F4.3 Importance of P arrival.
		105 4 F4.3 Importance of S arrival.
		109 1 A1 Data source code.
		110 1 A1 Label code for duration magnitude from FC1 or FC2 command.
		111 1 A1 Label code for amplitude magnitude from XC1 or XC2 command.
		112 2 A2 2-letter station location code (component extension).
		113 is the last filled column.

		Example HYPOINVERSE doc
			:
		19991201 0 8 64735 5775120 3094 355 0 160
		PST NC VVHZ IPD0199912010008 7.56 00 0 4 0 0
		PMM NC VVHZ EPU2199912010008 7.71 700 0 4 19M 0 0
		PVC NC VVHZ IPD1199912010008 7.81 8.98ESD2 00 0 0 6 0
		PPC NC VVHZ IPU0199912010008 8.13 9.19ESU2 1600 0 4 7M 0 0
		PHP NC VVHZ EPU2199912010008 8.39 00 0 4 0 0
		PSM NC VVHZ EPU2199912010008 9.98 00 0 4 0 0
		BMS NC VVHZ P 419991201000899.99 00 0 4 22 0
		21069577

Example INGV:

		200512132009226042 4387 12E4874   90  0 15137 24 11620767 507 7814 311298    203    0 301 469 12   0 120  0 15Ita WW   16    0  0   0  0    141429 298 120   0   01
		$1
		LNSS IV EBHE P U0200512132009 2792  62181    0   0   0      0 0  0   0   0 235 9200  0   59127315  0 498   0W     0
		$   6 5.20 1.80 6.40 2.65 0.06 NSR0  -47 PHP0  820 151886 19 943 23 497 31 329 39 159 47  91
		LNSS IV EBHE P U2200512132009 2794  64 90    0   0   0      0 0  0   0   0 235 9200  0   72127334  0 124   0W     0
		$   6 5.18 1.80 5.50 2.05 0.04 NSR0  -47 PHP0  954 151220 19 636 23 531 31 289 39 172 47 105
		MNS  IV EBHE P  3200512132009 2982 -75 36    0   0   0      0 0  0   0   0 399 9100  0   49196299  0  40   0W     0
		$   6 4.91 1.80 5.38 2.18 0.07 NSR0  -43 PHP0   38 15 525 19 483 23 258 31 168 39  80 43  66

*/

		if (check_for_S_arrival) {
			/* check for S phase input in last input line read */

			// check for zero or blank S phase time and remark
			// 42 5 F5.2 Second of S arrival.
			// 47 2 A2 S remark such as "ES".
			istat = ReadFortranString(line, 42, 7, chrtmp);
//printf("chrtmp <%s>\n", chrtmp);
			if (istat > 0
						 && strncmp(chrtmp, "     ", 5) != 0
						 && strncmp(chrtmp, "    0", 5) != 0
						 && strncmp(chrtmp + 5, "  ", 2) != 0)
			{
//printf("ACCEPT chrtmp <%s>\n", chrtmp);
				// read S phase input in last input line read
				istat = ReadFortranString(line, 1, 5, arrival->label);
				TrimString(arrival->label);
				istat = ReadFortranString(line, 6, 2, arrival->network);  // network code ignored in NLL
				TrimString(arrival->network);
				istat += ReadFortranString(line, 9, 1, arrival->comp);
				TrimString(arrival->comp);
				istat += ReadFortranString(line, 10, 3, arrival->inst);
				TrimString(arrival->inst);
				// try and decode S remark
				arrival->onset[0] = ARRIVAL_NULL_CHR;
				istat += ReadFortranString(line, 47, 2, chrtmp);
				if (chrtmp[0] == 'S' || chrtmp[0] == 's') {
					strcpy(arrival->phase, chrtmp);
					TrimString(arrival->phase);
				} else if (chrtmp[1] == 'S' || chrtmp[1] == 's') {
					arrival->onset[0] = chrtmp[0];
					arrival->phase[0] = chrtmp[1];
				} else {
					arrival->phase[0] = 'S';
				}
				//istat += ReadFortranString(line, 49, 1, arrival->first_mot);
				//TrimString(arrival->first_mot);
				istat += ReadFortranInt(line, 18, 4, &arrival->year);
				istat += ReadFortranInt(line, 22, 2, &arrival->month);
				istat += ReadFortranInt(line, 24, 2, &arrival->day);
				istat += ReadFortranInt(line, 26, 2, &arrival->hour);
				istat += ReadFortranInt(line, 28, 2, &arrival->min);
				istat += ReadFortranReal(line, 42, 5, &arrival->sec);
				// check for integer sec format
				if (arrival->sec > 99.999)
					arrival->sec /= 100.0;
				istat += ReadFortranReal(line, 30, 5, &psec);
				// check for integer sec format
				if (psec > 99.999)
					psec /= 100.0;
				// check for P second >= 60.0 - not needed (?)
				if (psec >= 60.0 && arrival->sec < 60.0)
					arrival->sec += 60.0;
				istat += ReadFortranInt(line, 50, 1, &arrival->quality);
				// read optional amplitude/period fields - read with P
				//istat += ReadFortranReal(line, 55, 7, &arrival->amplitude);
				//istat += ReadFortranReal(line, 84, 3, &arrival->period);
				//istat += ReadFortranReal(line, 88, 4, &arrival->coda_dur);
			}

			// check for valid S arrival input found
			if (istat == 12
						 && IsPhaseID(arrival->phase, "S")
						 && IsGoodDate(arrival->year, arrival->month, arrival->day)) {
				// convert quality to error
				Qual2Err(arrival);
				line[0] = '\0';
				check_for_S_arrival = 0;
				return(istat);
						 } else {  		// not found
							 check_for_S_arrival = 0;
							 return(OBS_FILE_SKIP_INPUT_LINE);
						 }

		}


// read next line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

// check for shawdow card - line starting with $
		if (strncmp(line, "$", 1) == 0)
			return(OBS_FILE_SKIP_INPUT_LINE);

// check for summary header - line starting with year
		if (ReadFortranInt(line, 1, 4, &idummy) == 1 && idummy > 0) {
			strcpy(HypoInverseArchiveSumHdr, line);
			//printf("HypoInverseArchiveSumHdr:\n|%s|\n", HypoInverseArchiveSumHdr);
			return(OBS_FILE_SKIP_INPUT_LINE);
		}

// read formatted P arrival input
		istat = ReadFortranString(line, 1, 5, arrival->label);
		TrimString(arrival->label);
		istat = ReadFortranString(line, 6, 2, arrival->network);  // network code ignored in NLL
		TrimString(arrival->network);
		istat += ReadFortranString(line, 9, 1, arrival->comp);
		TrimString(arrival->comp);
		istat += ReadFortranString(line, 10, 3, arrival->inst);
		TrimString(arrival->inst);
// try and decode P remark
		arrival->onset[0] = ARRIVAL_NULL_CHR;
		istat += ReadFortranString(line, 14, 2, chrtmp);
		if (chrtmp[0] == 'P' || chrtmp[0] == 'p') {
			strcpy(arrival->phase, chrtmp);
			TrimString(arrival->phase);
		} else if (chrtmp[1] == 'P' || chrtmp[1] == 'p') {
			arrival->onset[0] = chrtmp[0];
			arrival->phase[0] = chrtmp[1];
		} else {
			arrival->phase[0] = 'P';
		}
		TrimString(arrival->phase);
		istat += ReadFortranString(line, 16, 1, arrival->first_mot);
		TrimString(arrival->first_mot);
		istat += ReadFortranInt(line, 18, 4, &arrival->year);
		istat += ReadFortranInt(line, 22, 2, &arrival->month);
		istat += ReadFortranInt(line, 24, 2, &arrival->day);
		istat += ReadFortranInt(line, 26, 2, &arrival->hour);
		istat += ReadFortranInt(line, 28, 2, &arrival->min);
		istat += ReadFortranReal(line, 30, 5, &arrival->sec);
// check for integer sec format
		if (arrival->sec > 99.999)
			arrival->sec /= 100.0;
		istat += ReadFortranInt(line, 17, 1, &arrival->quality);

		if (istat != 12) {
			line[0] = '\0';
			return(OBS_FILE_END_OF_EVENT);
		}

// read optional amplitude/period fields
		istat += ReadFortranReal(line, 55, 7, &arrival->amplitude);
		istat += ReadFortranReal(line, 84, 3, &arrival->period);
		istat += ReadFortranReal(line, 88, 4, &arrival->coda_dur);


		/* check for valid phase code */
//		if (IsPhaseID(arrival->phase, "P")) {
			//strcpy(arrival->phase, "P");
		check_for_S_arrival = 1;

//		} else if (IsPhaseID(arrival->phase, "S")) {
			//strcpy(arrival->phase, "S");
//			check_for_S_arrival = 0;
//		} else
//			return(OBS_FILE_END_OF_EVENT);

		if (!IsGoodDate(arrival->year, arrival->month, arrival->day))
			return(OBS_FILE_END_OF_EVENT);

		// convert quality to error
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "INGV_BOLL") == 0 ||
			strcmp(ftype_obs, "INGV_BOLL_LOCAL") == 0 ||
			strcmp(ftype_obs, "INGV_ARCH") == 0)
	{
/*
		1 0
		DOI 1016   EZPG  03243563 EZSG  243882                             23
		11 0
		SEI 1016   EZPG  04132382 EZSG  132881
		VMG 1016   EZPG  04132514 EZSG  133068                             56
		ZCCA1016   EZPG  04132655                                          47
*/
		if (check_for_S_arrival > 0) {
			/* check for additional phase input in last input line read */

			ioff = 0;
			if (strcmp(ftype_obs, "INGV_ARCH") == 0)
				ioff = 13 * (check_for_S_arrival - 1);
			/* check for zero or blank S phase time */
			istat = ReadFortranString(line, 33 + ioff, 6, chrtmp);
			if (istat > 0
						 && strncmp(chrtmp, "      ", 6) != 0
						 && strncmp(chrtmp, "     0", 6) != 0)
			{
				/* read S phase input in last input line read */
				istat = ReadFortranString(line, 1, 4, arrival->label);
				TrimString(arrival->label);
				//istat += ReadFortranInt(line, 10, 2, &arrival->year);
			// no year in INGV boll!
				arrival->year = 2003;
				istat += ReadFortranInt(line, 5, 2, &arrival->month);
				istat += ReadFortranInt(line, 7, 2, &arrival->day);
				istat += ReadFortranInt(line, 18, 2, &arrival->hour);
				istat += ReadFortranInt(line, 33 + ioff, 2, &arrival->min);
				istat += ReadFortranReal(line, 35 + ioff, 4, &arrival->sec);
				arrival->sec /= 100.0;
				istat += ReadFortranReal(line, 22, 4, &psec);
				psec /= 100.0;
		// check for P second >= 60.0 (correct for INGV???)
				if (psec >= 60.0 && arrival->sec < 60.0)
					arrival->sec += 60.0;
				istat += ReadFortranString(line, 27 + ioff, 1, arrival->onset);
				// set weight
				arrival->quality = arrival->onset[0] == 'i' ? 1 : 2;
				istat += ReadFortranString(line, 28 + ioff, 1, arrival->comp);
				if (arrival->comp[0] == ' ')
					arrival->comp[0] = ARRIVAL_NULL_CHR;
				arrival->phase[0] = '\0';
				istat += ReadFortranString(line, 29 + ioff, 4, arrival->phase);
				TrimString(arrival->phase);
			}
		}

		/* check for S arrival input found */
		if (check_for_S_arrival > 0 && istat == 10
//				&& IsPhaseID(arrival->phase, "S")
				  && IsGoodDate(arrival->year,
				arrival->month, arrival->day)) {

			//strcpy(arrival->phase, "S");

					/* set error fields */
					strcpy(arrival->error_type, "GAU");
					if (arrival->quality >= 0 &&
									       arrival->quality < NumQuality2ErrorLevels) {
						arrival->error =
								Quality2Error[arrival->quality];
									       } else {
										arrival->error =
										Quality2Error[NumQuality2ErrorLevels - 1];
										puterr("WARNING: invalid arrival weight.");
									       }

			// increment additional arrival count
									       if (strcmp(ftype_obs, "INGV_ARCH") == 0 && check_for_S_arrival < 3) {
										check_for_S_arrival++;
									       } else {
										check_for_S_arrival = 0;
										line[0] = '\0';
									       }
									       return(istat);
				} else if (check_for_S_arrival > 0) {
					check_for_S_arrival = 0;
					return(OBS_FILE_SKIP_INPUT_LINE);
				}



/*
				1 0
				DOI 1016   EZPG  03243563 EZSG  243882                             23
				11 0
				SEI 1016   EZPG  04132382 EZSG  132881
				VMG 1016   EZPG  04132514 EZSG  133068                            156
				ZCCA1016   EZPG  04132655                                          47
*/
		// read next line
				cstat = fgets(line, MAXLINE_LONG, fp_obs);

				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);

				/* read formatted P (or S) arrival input */
				istat = ReadFortranString(line, 1, 4, arrival->label);
				if (isspace(arrival->label[0])) {
			// check to skip next event(s) if is teleseism
					if (strcmp(ftype_obs, "INGV_BOLL_LOCAL") == 0) {
						while(1) {
							TrimString(line);
							if (strlen(line) > 10) {
								while(1) {		// read to next event
									cstat = fgets(line, MAXLINE_LONG, fp_obs);
									if (cstat == NULL)
										return(OBS_FILE_END_OF_INPUT);
									if(isspace(line[0]))
										break;
								}
							} else {
								break;
							}
						}
					}
					line[0] = '\0';
					return(OBS_FILE_END_OF_EVENT);
				}
				TrimString(arrival->label);
				istat += ReadFortranString(line, 12, 1, arrival->onset);
				TrimString(arrival->onset);
				istat += ReadFortranString(line, 13, 1, arrival->comp);
				if (arrival->comp[0] == ' ')
					arrival->comp[0] = ARRIVAL_NULL_CHR;
		// set weight
				arrival->quality = arrival->onset[0] == 'i' ? 0 : 1;
				istat += ReadFortranString(line, 14, 4, arrival->phase);
				TrimString(arrival->phase);
				istat += ReadFortranString(line, 10, 1, arrival->first_mot);
				TrimString(arrival->first_mot);
	// no year in INGV boll!
				arrival->year = 2003;
				istat += ReadFortranInt(line, 5, 2, &arrival->month);
				istat += ReadFortranInt(line, 7, 2, &arrival->day);
				istat += ReadFortranInt(line, 18, 2, &arrival->hour);
				istat += ReadFortranInt(line, 20, 2, &arrival->min);
				istat += ReadFortranReal(line, 22, 4, &arrival->sec);
				arrival->sec /= 100.0;


				if (istat != 10) {
					line[0] = '\0';
					return(OBS_FILE_END_OF_EVENT);
				}

				/* read optional amplitude/period fields */
				istat += ReadFortranReal(line, 66, 4, &arrival->amplitude);
//printf("AMPLITUDE: %lf (%s)\n", arrival->amplitude, line);
		//istat += ReadFortranReal(line, 48, 3, &arrival->period);
// 20030826 AJL coda_dur added
		//istat += ReadFortranReal(line, 71, 5, &arrival->coda_dur);


				check_for_S_arrival = 1;
				/* check for valid phase code */
//		if (IsPhaseID(arrival->phase, "P")) {
			//strcpy(arrival->phase, "P");
//			check_for_S_arrival = 1;

//		} else if (IsPhaseID(arrival->phase, "S")) {
			//strcpy(arrival->phase, "S");
//			check_for_S_arrival = 0;
//		} else
//			return(OBS_FILE_END_OF_EVENT);

				if (!IsGoodDate(arrival->year, arrival->month, arrival->day))
					return(OBS_FILE_END_OF_EVENT);


				/* check for integer sec format */
				if (arrival->sec > 99.999)
					arrival->sec /= 100.0;

				/* convert quality to error */
				Qual2Err(arrival);

				return(istat);
	}



	else if (strcmp(ftype_obs, "NCSN_Y2K_5") == 0)
	{

		if (check_for_S_arrival) {
			/* check for S phase input in last input line read */

			/* check for zero or blank S phase time */
			istat = ReadFortranString(line, 48, 1, chrtmp);
			if (istat > 0 && (strncmp(chrtmp, "S", 1) == 0 || strncmp(chrtmp, "s", 1) == 0))
			{
				/* read S phase input in last input line read */
				istat = ReadFortranString(line, 1, 5, arrival->label);
				TrimString(arrival->label);
				istat += ReadFortranInt(line, 18, 4, &arrival->year);
				istat += ReadFortranInt(line, 22, 2, &arrival->month);
				istat += ReadFortranInt(line, 24, 2, &arrival->day);
				istat += ReadFortranInt(line, 26, 2, &arrival->hour);
				istat += ReadFortranInt(line, 28, 2, &arrival->min);
				istat += ReadFortranReal(line, 42, 5, &arrival->sec);
				/* check for integer sec format */
//				if (arrival->sec > 99.999)
				arrival->sec /= 100.0;
				istat += ReadFortranReal(line, 30, 5, &psec);
				psec /= 100.0;
				/* check for P second >= 60.0 */
				if (psec >= 60.0 && arrival->sec < 60.0)
					arrival->sec += 60.0;
				arrival->phase[0] = '\0';
				istat += ReadFortranString(line, 48, 1, arrival->phase);
				TrimString(arrival->phase);
				istat += ReadFortranString(line, 47, 1, arrival->onset);
				TrimString(arrival->onset);
				istat += ReadFortranInt(line, 50, 1, &arrival->quality);
			}
		}

		/* check for S arrival input found */
		if (check_for_S_arrival && istat == 11
			&& IsPhaseID(arrival->phase, "S")
			&& IsGoodDate(arrival->year, arrival->month, arrival->day)) {

			/* set error fields */
			strcpy(arrival->error_type, "GAU");
			if (arrival->quality >= 0 && arrival->quality < NumQuality2ErrorLevels) {
				arrival->error = Quality2Error[arrival->quality];
			} else {
				arrival->error = Quality2Error[NumQuality2ErrorLevels - 1];
				puterr("WARNING: invalid arrival weight.");
			}

			line[0] = '\0';
			check_for_S_arrival = 0;

			// AJL 20070608 - this format may have muliple entries for each phase, need to reject
			// earlier phases with large error so that later entries will be used
			if (arrival->error > 999.0)
				return(OBS_FILE_SKIP_INPUT_LINE);

			return(istat);
		} else if (check_for_S_arrival) {
			check_for_S_arrival = 0;
			return(OBS_FILE_SKIP_INPUT_LINE);
		}


		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		if (strcmp(line, "    ") == 0) {
			line[0] = '\0';
			return(OBS_FILE_END_OF_EVENT);
		}

		/* read formatted P (or S) arrival input */
		istat = ReadFortranString(line, 1, 5, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 9, 1, arrival->comp);
		TrimString(arrival->comp);
		istat += ReadFortranString(line, 10, 3, arrival->inst);
		TrimString(arrival->inst);
		istat += ReadFortranString(line, 14, 1, arrival->onset);
		TrimString(arrival->onset);
		if (strpbrk(arrival->onset, "XYZ") != NULL) {	// skip RTP (phases with X,Y,Z onset)
			return(OBS_FILE_SKIP_INPUT_LINE);
		}
		istat += ReadFortranString(line, 15, 1, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranString(line, 16, 1, arrival->first_mot);
		TrimString(arrival->first_mot);
		istat += ReadFortranInt(line, 17, 1, &arrival->quality);
/*
		if (arrival->quality > 3) {			// skip phases with quality > 4
		return(OBS_FILE_SKIP_INPUT_LINE);
}
*/
		istat += ReadFortranInt(line, 18, 4, &arrival->year);
		istat += ReadFortranInt(line, 22, 2, &arrival->month);
		istat += ReadFortranInt(line, 24, 2, &arrival->day);

		if (!IsGoodDate(arrival->year, arrival->month, arrival->day))
			return(OBS_FILE_END_OF_EVENT);

		istat += ReadFortranInt(line, 26, 2, &arrival->hour);
		istat += ReadFortranInt(line, 28, 2, &arrival->min);
		istat += ReadFortranReal(line, 30, 5, &arrival->sec);
		/* check for integer sec format */
//		if (arrival->sec > 99.999)
		arrival->sec /= 100.0;

		if (istat != 13) {
			line[0] = '\0';
			return(OBS_FILE_END_OF_EVENT);
		}

		/* read optional amplitude/period fields */
		istat += ReadFortranReal(line, 55, 7, &arrival->amplitude);
		arrival->amplitude /= 100.0;
		istat += ReadFortranReal(line, 84, 3, &arrival->period);
		arrival->period /= 100.0;


		/* check for valid phase code */
		if (IsPhaseID(arrival->phase, "P")) {
//strcpy(arrival->phase, "P");
			check_for_S_arrival = 1;

		} else if (IsPhaseID(arrival->phase, "S")) {
//strcpy(arrival->phase, "S");
			check_for_S_arrival = 0;
		} else
			return(OBS_FILE_SKIP_INPUT_LINE);

		/* convert quality to error */
		Qual2Err(arrival);

		// AJL 20070608 - this format may have muliple entries for each phase, need to reject
		// earlier phases with large error so that later entries will be used
		if (arrival->error > 999.0)
			return(OBS_FILE_SKIP_INPUT_LINE);

		return(istat);

	}



	else if (strcmp(ftype_obs, "PAOLO_OV") == 0 ||
			strcmp(ftype_obs, "ALBERTO_3D") == 0 ||
			strcmp(ftype_obs, "ALBERTO_3D_4") == 0 ||
			strcmp(ftype_obs, "SIMULPS") == 0 ||
			strcmp(ftype_obs, "VELEST") == 0 )
	{
/*
		"PAOLO_OV"
		87 119 23 5 45.00 40 49.17  14 25.80   0.43   0.00
		BKEP0 45.51 BKWP0 45.61 BKNP0 45.66 BAFP0 45.74 BKES1 45.86 BKWS1 46.04
		BKNS1 46.06 BAFS1 46.20

		"ALBERTO_3D"
		891018  0 4 15.34 37  2.66 121 52.54  16.92   5.80
		JTGVIPU018.3000JBZVIPU018.7600JECVIPD018.5700JRGVIPU018.4500JHLVIPD018.5500
		HLTVIPU024.9600BSLVIPU025.5800BPCVEPD025.4900HJSVIPU025.4900

		"SIMULPS"
		from Stephan Husen   email:  stephan@tomo.ig.erdw.ethz.ch
		summary line containing event origin time, location, and magnitude
		format: a4,a2,1x,a2,i2,1x,f5.2,i3,a1,f5.2,1x,i3,a1,f5.2,f7.2,f7.2)
		iyrmo, iday, ihr, mino, seco, ltde, cns, eltm, lnde, cew, elnm, dep, mag
		observed traveltimes to stations, six sets per line
		each set is in the format: a4,a4,f6.2       sta, rmki, tt        rmki -> (I,E)(P,S,SP)(U,D,-)(0-4)
		blank terminates event's data
example:
		84 3 6  6 7 58.58 46N19.78   7E26.85   3.77   2.00
		SIE_IP-1  1.45DIX2IP-1  4.94DIX_IP-1  4.95EMS_IP-1  8.14EMV_IP-1  8.54MMK2IP-1  8.58
		MMK_IP-1  8.67STG_IP-1 16.48SLE_IP-1 27.54WIL_IP-1 28.14SAX_IP-1 30.78

		84 3 7  4 7 56.92 46N20.89   7E26.59   6.09   1.50
		ZZB_IP-1  1.03ZZE_IP-1  1.06ZZA_IP-1  1.07ZZC_IP-1  1.11ZZD_IP-1  1.11ZZF_IP-1  1.11

		"VELEST"
		phase format of VELEST program; from Stephan Husen email: stephan@tomo.ig.erdw.ethz.ch
		summary line containing event origin time, location, and magnitude
		format:a4,a2,1x,a2,i2,1x,f5.2,1x,f7.4,a1,1x,f8.4,a1,f7.2,f7.2
		iyrmo, iday, ihr, mino, seco, ltde, cns, lnde, cew, dep, mag
		observed travel times (six sets per line)
		each set is in format a4,a1,a1,f6.2
		sta, phaseID, weight, tt

*/
		/* check for end of event (assumes no blanks after last phase) */
		chr = fgetc(fp_obs);
		if (chr == EOF) {
			return(OBS_FILE_END_OF_INPUT);
		} else if (chr == '\n') {
			if ((chr = fgetc(fp_obs)) != EOF && !isdigit(chr)) {
				// another phase follows
				ungetc(chr, fp_obs);
			} else if (chr == '0') {
				// end of event "0" line
				// read to end of line
				while ((chr = fgetc(fp_obs)) != EOF && chr != '\n')
					;
				return(OBS_FILE_END_OF_EVENT);
			} else {
				// end of event
				ungetc(chr, fp_obs);
				return(OBS_FILE_END_OF_EVENT);
			}
		} else {
			ungetc(chr, fp_obs);
		}


		/* check for event hypocenter line */
		if ((chr = fgetc(fp_obs)) != EOF && isdigit(chr)) {
			// read hypocenter time
			ungetc(chr, fp_obs);
			/* read next line */
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			if (cstat == NULL)
				return(OBS_FILE_END_OF_INPUT);
			istat = ReadFortranInt(line, 1, 2, &EventTime.year);
			istat += ReadFortranInt(line, 3, 2, &EventTime.month);
			istat += ReadFortranInt(line, 5, 2, &EventTime.day);
			istat += ReadFortranInt(line, 8, 2, &EventTime.hour);
			istat += ReadFortranInt(line, 10, 2, &EventTime.min);
			if (istat == 5) {
	// temporary fix for HYPO71 Y2K problem
				if (EventTime.year < 20)
					EventTime.year += 100;
				EventTime.year += 1900;
				// read to end of line
				//while ((chr = fgetc(fp_obs)) != EOF && chr != '\n')
				//	;
				if (chr == EOF)
					return(OBS_FILE_END_OF_INPUT);
			}
		} else {
			if (chr == EOF)
				return(OBS_FILE_END_OF_INPUT);
			ungetc(chr, fp_obs);
		}


		if (strcmp(ftype_obs, "PAOLO_OV") == 0) {
		// try to read phase arrival input
			istat = fscanf(fp_obs, " %s %lf", line, &arrival->sec);
			if (istat == EOF)
				return(OBS_FILE_END_OF_INPUT);
			istat += ReadFortranString(line, 1, 3, arrival->label);
			istat += ReadFortranString(line, 4, 1, arrival->phase);
			istat += ReadFortranInt(line, 5, 1, &arrival->quality);
			if (istat != 5)
				return(OBS_FILE_END_OF_EVENT);

		} else if (strcmp(ftype_obs, "SIMULPS") == 0) {
			// try to read phase arrival input
			istat = fscanf(fp_obs, "%14c", line);
			line[14]= '\0';
			if (istat == EOF)
				return(OBS_FILE_END_OF_INPUT);
// printf("<%s>\n", line);
			istat += ReadFortranString(line, 1, 4, arrival->label);
			istat += ReadFortranString(line, 5, 1, arrival->onset);
			istat += ReadFortranString(line, 6, 1, arrival->phase);
			istat += ReadFortranString(line, 7, 1, arrival->first_mot);
			istat += ReadFortranInt(line, 8, 1, &arrival->quality);
			istat += ReadFortranReal(line, 9, 6, &arrival->sec);

			if (istat != 7)
				return(OBS_FILE_END_OF_EVENT);

		} else if (strcmp(ftype_obs, "VELEST") == 0) {
			/* try to read phase arrival input */
			istat = fscanf(fp_obs, "%12c", line);
			line[12]= '\0';
			if (istat == EOF)
				return(OBS_FILE_END_OF_INPUT);
			/* printf("<%s>\n", line); */
			istat += ReadFortranString(line, 1, 4, arrival->label);
			istat += ReadFortranString(line, 5, 1, arrival->phase);
			istat += ReadFortranInt(line, 6, 1, &arrival->quality);
			istat += ReadFortranReal(line, 7, 6, &arrival->sec);

			if (istat != 5)
				return(OBS_FILE_END_OF_EVENT);

		} else if (strcmp(ftype_obs, "ALBERTO_3D") == 0
					 || strcmp(ftype_obs, "ALBERTO_3D_4") == 0) {
			// try to read phase arrival input
						 istat = fscanf(fp_obs, "%15c", line);
						 line[15]= '\0';
						 if (istat == EOF)
							 return(OBS_FILE_END_OF_INPUT);
//printf("<%s>\n", line);
						 if (strcmp(ftype_obs, "ALBERTO_3D_4") == 0) {
							 istat += ReadFortranString(line, 1, 4, arrival->label);
							 istat++;
						 } else {
							 istat += ReadFortranString(line, 1, 3, arrival->label);
							 istat += ReadFortranString(line, 4, 1, arrival->comp);
						 }
						 istat += ReadFortranString(line, 5, 1, arrival->onset);
						 istat += ReadFortranString(line, 6, 1, arrival->phase);
						 istat += ReadFortranString(line, 7, 1, arrival->first_mot);
						 istat += ReadFortranInt(line, 8, 1, &arrival->quality);
						 istat += ReadFortranReal(line, 9, 7, &arrival->sec);

						 if (istat != 8)
							 return(OBS_FILE_END_OF_EVENT);
					 }


					 arrival->year = EventTime.year;
					 arrival->month = EventTime.month;
					 arrival->day = EventTime.day;
					 arrival->hour = EventTime.hour;
					 arrival->min = EventTime.min;


					 /* convert quality to error */
					 Qual2Err(arrival);

					 return(istat);
	}



	else if (strcmp(ftype_obs, "SED_LOC") == 0 || strcmp(ftype_obs, "SED_LOC_ERR") == 0)
	{

/* example:
		SH 06/11/2005 LOC format has been extended to include uncertainty interval for picking
		SH 23/11/2005 LOC format has been extended to include observation weight (quality) in last column

		1 1   200.0   300.0    1.71 1                        INST
		0.000     0.000  10.00 0000 00 00 00 00  0.00 0    TRIAL
		nico                                                  AUTOR
		2002/01/04 00:01                                      Local            20020015
		FUSIO   P       ID  27.454 1 0.21      186 NWA       0 Pick HHN  29.626  -0.050   0.050 0
		FUSIO   S       E   29.474 1 0.21      186 NWA       0 Pick HHN  29.626  -0.250   0.250 1
		VDL     P       ID  33.578 1 0.24       16 NWA       0 Pick HHN  41.014  -0.050   0.050 1
		VDL     S       Q   39.651 1 0.24       16 NWA       0 Pick HHN  41.014  -0.500   0.500 3
		LLS     P       IU  34.752 1 0.36       14 NWA       0 Pick HHE  43.193  -0.050   0.050 2
		LLS     S       I   41.622 1 0.36       14 NWA       0 Pick HHE  43.193  -0.050   0.050 1
		BNALP   P       Q   35.959 1 0.59        8 NWA       0 Pick HHE  47.417  -0.500   0.500 3
		BNALP   S       E   43.504 1 0.59        8 NWA       0 Pick HHE  47.417  -0.250   0.250 2
		HASLI   P       E   36.250 1 0.36       32 NWA       0 Pick HHE  44.465  -0.250   0.250 2
		HASLI   S       E   44.209 1 0.36       32 NWA       0 Pick HHE  44.465  -0.250   0.250 2
		MMK     P       E   36.365 1 0.22       32 NWA       0 Pick HHN  46.943  -0.250   0.250 2
		LKBD    P       E   39.858 1 1.77       18 NWA       0 Pick HHN  53.318  -0.250   0.250 2
		BERNI   P       E   40.349 1 0.58       10 NWA       0 Pick HHN  56.013  -0.250   0.250 2
		CHDAW   P       E   40.741 1 0.29       14 NWA       0 Pick HHN  59.776  -0.250   0.250 2
		FUORN   P       IU  44.069 1 0.25       10 NWA       0 Pick HHE  65.863  -0.050   0.050 0
		SKIP
		20020104000124246358N008807E00515Ml1272705135008005022SEDN023156015414         A
		KP200201040001                                        ARCHIVE
*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}


		// read event hypocenter line
		if (!in_hypocenter_event) {
			// find origin time line
			ifound = 0;
			while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0) {
		/* SH 03/05/2003
				read control parameters, which are specified in first line of SED_LOC format
				so far only VpVSRatio is of use
				the value of VpVsRatio specified in NLLoc control file will be overwritten!
				SH 11/25/2004
				VpVsRatio should only be read from LOCfile if NLLoc is used within SNAP */
#ifdef CUSTOM_ETH
				if (strcmp(eth_line_key, "INST") == 0) {
					istat = ReadFortranReal(line, 23, 6, &vpvs);
					if (istat != 1) {
						puterr(
								"WARNING: could not read VpVsRatio! Use value of control file\n");
					} else {
						VpVsRatio = vpvs;
					}
				}
#endif
/* SH 07/26/2004
				other identiers for SED_LOC are Tele and Unkn  */
				if ((strcmp(eth_line_key, "Loca") == 0) ||
								 (strcmp(eth_line_key, "Tele") == 0) ||
								 (strcmp(eth_line_key, "Unkn") == 0) ) {
					ifound = 1;
					break;
								 }
				// read next line
								 cstat = fgets(line, MAXLINE_LONG, fp_obs);
								 if (cstat == NULL)
									 return(OBS_FILE_END_OF_INPUT);
			}
			if (!ifound)
				return(OBS_FILE_END_OF_EVENT);

			// read hypocenter time
			//2001/12/04 01:29                                      Local           20010748
			istat = ReadFortranInt(line, 1, 4, &EventTime.year);
			istat += ReadFortranInt(line, 6, 2, &EventTime.month);
			istat += ReadFortranInt(line, 9, 2, &EventTime.day);
			istat += ReadFortranInt(line, 12, 2, &EventTime.hour);
			istat += ReadFortranInt(line, 15, 2, &EventTime.min);
			/* SH 03/05/2004  added event number  */
			istat += ReadFortranInt(line, 71, 8, &EventTime.ev_nr);
			/* SH 23/11/2004  event number is undertermined when reading GSE2 files */
			if (istat == 4 || istat == 6) {
				if (istat == 4)
					EventTime.ev_nr = 0;  /* no event number */
				in_hypocenter_event = 1;
				// read until phase line reached
				while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
								   && strcmp(line, "    ") != 0
								   /* SH 06/11/2005  change to deal with extended LOC format */
								   && strcmp(eth_line_key," Pic") != 0 ) {
					cstat = fgets(line, MAXLINE_LONG, fp_obs);
					if (cstat == NULL)
						return(OBS_FILE_END_OF_INPUT);
								   }
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}

		// check if valid phase line: key is blank or strlen < 55
		if ((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
				   && strcmp(line, "    ") != 0
				   /* SH 06/11/2005  change to deal with extended LOC format */
				   && strcmp(eth_line_key," Pic") != 0 ) {
			return(OBS_FILE_SKIP_INPUT_LINE);
				   }

		/* read phase arrival input
				   SH 06/11/2005 LOC format has been extended to include uncertainty interval for picking
				   SH 23/11/2005 LOC format has been extended to include obs weight in the last coluumn
				   FUSIO   P       ID  27.454 1 0.21      186 NWA       0 Pick HHN  29.626  -0.050   0.050 1
				   FUSIO   S       E   29.474 1 0.21      186 NWA       0 Pick HHN  29.626  -0.250   0.250 3
		*/
				   istat = ReadFortranString(line, 1, 5, arrival->label);
				   istat += ReadFortranString(line, 9, 6, arrival->phase);
				   istat += ReadFortranString(line, 17, 1, arrival->onset);
				   istat += ReadFortranString(line, 18, 1, arrival->first_mot);
				   istat += ReadFortranReal(line, 20, 7, &arrival->sec);
//		istat += ReadFortranReal(line, 21, 6, &arrival->sec);
				   istat += ReadFortranInt(line, 28, 1, &eth_use_loc);
				   istat += ReadFortranReal(line, 29, 5, &arrival->period);
				   istat += ReadFortranReal(line, 34, 9, &arrival->amplitude);
/* SH 02/25/2004 bug fix
				   arrival->inst is only of type char[5] and not char[9]!
				   istat += ReadFortranString(line, 44,9, arrival->inst);  */
				   istat += ReadFortranString(line, 44,4, arrival->inst);
				   if (strcmp(arrival->inst, "    ") == 0)
					   strcpy(arrival->inst, ARRIVAL_NULL_STR);
				   istat += ReadFortranInt(line, 54, 1, &arrival->clipped);
				   /* SH 23/11/2005 read arrival quality from last column (for extended LOC format) */
				   if (strcmp(eth_line_key," Pic") == 0)
					   if ( (ReadFortranInt(line, 89, 1, &arrival->quality)) > 0 ) istat++;


				   if (istat < 10) {
					   return(OBS_FILE_END_OF_EVENT);
				   }

				   /* remove blanks/whitespace */
				   removeSpace(arrival->label);
				   removeSpace(arrival->phase);
				   removeSpace(arrival->inst);

				   /* AJL EvalPhaseID not used here anymore, used later */
/*		if (EvalPhaseID(arrival->phase) < 0) {
				   puterr2("WARNING: phase ID not found", arrival->phase);
				   return(OBS_FILE_INVALID_PHASE);
	}
*/

				   arrival->year = EventTime.year;
				   arrival->month = EventTime.month;
				   arrival->day = EventTime.day;
				   arrival->hour = EventTime.hour;
				   arrival->min = EventTime.min;

				   /* convert onset to error */
//Uncertainties of phase readings:
//                              I             E            Q
//for Key 'Loca':            < +/- 0.05   < +/- 0.2     > +/- 0.2
//for Key 'Regi' or 'Tele'   < +/- 0.20   < +/- 1.0     > +/- 1.0

/* SH 23/11/2005 for extended LOC format arrival quality is now in the last column;
				   arrival onset should no longer be used to estimate arrival error */

				   // AJL 20070308 added SED_LOC_ERR
				   if (strcmp(ftype_obs, "SED_LOC_ERR") == 0) {
					   istat = ReadFortranReal(line, 73, 7, &left_uncertainty);
					   istat += ReadFortranReal(line, 81, 7, &right_uncertainty);
					   strcpy(arrival->error_type, "GAU");
					   arrival->error = (right_uncertainty - left_uncertainty) / 2.0;;
				   } else if (istat == 11) {  /* extended LOC format */
					   if (eth_use_loc == 0) {
						   arrival->error = 9999.9;
					   }  else {
						   Qual2Err(arrival); }  /* convert quality to uncertainty in s */
				   } else {  /* old LOC format */
					   strcpy(arrival->error_type, "GAU");
					   if (eth_use_loc == 0) {
						   arrival->error = 9999.9;
					   } else if (strcmp(arrival->onset, "I") == 0) {
						   arrival->error = 0.05;
					   } else if (strcmp(arrival->onset, "E") == 0) {
						   arrival->error = 0.2;
					   } else if (strcmp(arrival->onset, "Q") == 0) {
						   arrival->error = 0.2;
					   } else {
						   arrival->error = 9999.9;
					   }
					   arrival->quality = 0;
				   }

				   return(istat);
	}





	else if (strcmp(ftype_obs, "SED_LOC_OLD") == 0)
	{

/* example:
		1 1   200.0   300.0    1.71 1                        INST
		0.000     0.000  10.00 0000 00 00 00 00  0.00 0    TRIAL
		maraini                                               AUTOR
		2001/12/04 01:29                                      Local           20010748
		FUORN   P       E   14.910 1 0.14      448 NWA       0
		FUORN   S       E   16.140 1 0.14      448 NWA       0
		OSS2    P       ID  16.428 1 0.09     2554 AHP       0
		OSS     P       ED  16.524 1 0.11     1672 AHP       0
		BERNI   P       E   17.930 1 0.18       46 NWA       0
		BERNI   S       E   21.139 1 0.18       46 NWA       0
		CHDAW   P       Q   20.016 1 0.56       10 NWA       0
		VDL2    P       IU  23.704 1 0.20      246 AHP       0
		VDL     P       E   23.863 1 0.12      188 AHP       0
		VDL     P       E   23.822 1 0.28        4 NWA       1
		VDL     S       E   31.177 1 0.28        4 NWA       1
		SKIP
		20011204012913146568N010226E00614Ml1239814161001001000SEDG024196006511         B
		KP200112040128                                        ARCHIVE
*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}


		// read event hypocenter line
		if (!in_hypocenter_event) {
			// find origin time line
			ifound = 0;
			while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0) {
		/* SH 03/05/2003
				read control parameters, which are specified in first line of SED_LOC format
				so far only VpVSRatio is of use
				the value of VpVsRatio specified in NLLoc control file will be overwritten!  */
				if (strcmp(eth_line_key, "INST") == 0) {
					istat = ReadFortranReal(line, 23, 6, &vpvs);
					if (istat != 1) {
						puterr(
								"WARNING: could not read VpVsRatio! Use value of control file\n");
					} else {
						VpVsRatio = vpvs;
					}
				}
				if (strcmp(eth_line_key, "Loca") == 0) {
					ifound = 1;
					break;
				}
				// read next line
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			}
			if (!ifound)
				return(OBS_FILE_END_OF_EVENT);

			// read hypocenter time
			//2001/12/04 01:29                                      Local           20010748
			istat = ReadFortranInt(line, 1, 4, &EventTime.year);
			istat += ReadFortranInt(line, 6, 2, &EventTime.month);
			istat += ReadFortranInt(line, 9, 2, &EventTime.day);
			istat += ReadFortranInt(line, 12, 2, &EventTime.hour);
			istat += ReadFortranInt(line, 15, 2, &EventTime.min);
			/* SH 03/05/2004  added event number  */
			istat += ReadFortranInt(line, 71, 8, &EventTime.ev_nr);
			if (istat == 6) {
				in_hypocenter_event = 1;
				// read until phase line reached
				while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
								   && strcmp(line, "    ") != 0) {
					cstat = fgets(line, MAXLINE_LONG, fp_obs);
					if (cstat == NULL)
						return(OBS_FILE_END_OF_INPUT);
								   }
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}

		// check if valid phase line: key is blank or strlen < 55
		if ((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
				   && strcmp(line, "    ") != 0) {
			return(OBS_FILE_SKIP_INPUT_LINE);
				   }

		// read phase arrival input
		//CHDAW   P       Q   20.016 1 0.56       10 NWA       0
		//OSS     P       ED  16.524 1 0.11     1672 AHP       0
		//BERNI   P       E   17.930 1 0.18       46 NWA       0

				   istat = ReadFortranString(line, 1, 5, arrival->label);
				   istat += ReadFortranString(line, 9, 6, arrival->phase);
				   istat += ReadFortranString(line, 17, 1, arrival->onset);
				   istat += ReadFortranString(line, 18, 1, arrival->first_mot);
				   istat += ReadFortranReal(line, 20, 7, &arrival->sec);
//		istat += ReadFortranReal(line, 21, 6, &arrival->sec);
				   istat += ReadFortranInt(line, 28, 1, &eth_use_loc);
				   istat += ReadFortranReal(line, 29, 5, &arrival->period);
				   istat += ReadFortranReal(line, 34, 9, &arrival->amplitude);
/* SH 02/25/2004 bug fix
				   arrival->inst is only of type char[5] and not char[9]!
				   istat += ReadFortranString(line, 44,9, arrival->inst);  */
				   istat += ReadFortranString(line, 44,4, arrival->inst);
				   /* SH 07232004 added */
		//istat += ReadFortranInt(line, 54, 1, &eth_use_mag);
				   istat += ReadFortranInt(line, 54, 1, &arrival->clipped);


				   if (istat != 10) {
					   return(OBS_FILE_END_OF_EVENT);
				   }

				   /* remove blanks/whitespace */
				   removeSpace(arrival->label);
				   removeSpace(arrival->phase);
				   removeSpace(arrival->inst);

				   /* AJL EvalPhaseID not used here anymore, used later */
/*		if (EvalPhaseID(arrival->phase) < 0) {
				   puterr2("WARNING: phase ID not found", arrival->phase);
				   return(OBS_FILE_INVALID_PHASE);
	}
*/

				   arrival->year = EventTime.year;
				   arrival->month = EventTime.month;
				   arrival->day = EventTime.day;
				   arrival->hour = EventTime.hour;
				   arrival->min = EventTime.min;

				   /* convert onset to error */
//Uncertainties of phase readings:
//                              I             E            Q
//for Key 'Loca':            < +/- 0.05   < +/- 0.2     > +/- 0.2
//for Key 'Regi' or 'Tele'   < +/- 0.20   < +/- 1.0     > +/- 1.0
				   strcpy(arrival->error_type, "GAU");
				   if (eth_use_loc == 0) {
					   arrival->error = 9999.9;
				   } else if (strcmp(arrival->onset, "I") == 0) {
					   arrival->error = 0.05;
				   } else if (strcmp(arrival->onset, "E") == 0) {
					   arrival->error = 0.2;
				   } else if (strcmp(arrival->onset, "Q") == 0) {
					   arrival->error = 0.2;
				   } else {
					   arrival->error = 9999.9;
				   }
				   arrival->quality = 0;

		// check if should use amplitude
				   if (eth_use_mag != 0)
					   arrival->amplitude = AMPLITUDE_NULL;

				   return(istat);
	}



//  ETH_LOC replaced by SED_LOC
	//else if (strcmp(ftype_obs, "ETH_LOC") == 0)
	else if (0)
	{

/* example:
		1 1   200.0   300.0    1.71 1                        INST
		0.000     0.000  10.00 0000 00 00 00 00  0.00 0    TRIAL
		maraini                                               AUTOR
		2001/12/04 01:29                                      Local           20010748
		FUORN   P       E   14.910 1 0.14      448 NWA       0
		FUORN   S       E   16.140 1 0.14      448 NWA       0
		OSS2    P       ID  16.428 1 0.09     2554 AHP       0
		OSS     P       ED  16.524 1 0.11     1672 AHP       0
		BERNI   P       E   17.930 1 0.18       46 NWA       0
		BERNI   S       E   21.139 1 0.18       46 NWA       0
		CHDAW   P       Q   20.016 1 0.56       10 NWA       0
		VDL2    P       IU  23.704 1 0.20      246 AHP       0
		VDL     P       E   23.863 1 0.12      188 AHP       0
		VDL     P       E   23.822 1 0.28        4 NWA       1
		VDL     S       E   31.177 1 0.28        4 NWA       1
		SKIP
		20011204012913146568N010226E00614Ml1239814161001001000SEDG024196006511         B
		KP200112040128                                        ARCHIVE
*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}


		// read event hypocenter line
		if (!in_hypocenter_event) {
			// find origin time line
			ifound = 0;
			while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0) {
				if (strcmp(eth_line_key, "Loca") == 0
								|| strcmp(eth_line_key, "Regi") == 0
								|| strcmp(eth_line_key, "Tele") == 0) {
					ifound = 1;
					break;
								}
				// read next line
								cstat = fgets(line, MAXLINE_LONG, fp_obs);
								if (cstat == NULL)
									return(OBS_FILE_END_OF_INPUT);
			}
			if (!ifound)
				return(OBS_FILE_END_OF_EVENT);

			// read hypocenter time
			//2001/12/04 01:29                                      Local           20010748
			istat = ReadFortranInt(line, 1, 4, &EventTime.year);
			istat += ReadFortranInt(line, 6, 2, &EventTime.month);
			istat += ReadFortranInt(line, 9, 2, &EventTime.day);
			istat += ReadFortranInt(line, 12, 2, &EventTime.hour);
			istat += ReadFortranInt(line, 15, 2, &EventTime.min);
//printf("EventTime: %d/%d/%d %d:%d\n",
//EventTime.year, EventTime.month, EventTime.day,
//EventTime.hour, EventTime.min);
			if (istat == 5) {
				in_hypocenter_event = 1;
				// read until phase line reached
				while((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
								   && strcmp(line, "    ") != 0) {
					cstat = fgets(line, MAXLINE_LONG, fp_obs);
					if (cstat == NULL)
						return(OBS_FILE_END_OF_INPUT);
								   }
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}

		// check if valid phase line: key is blank or strlen < 55
		if ((istat = ReadFortranString(line, 55, 4, eth_line_key)) > 0
				   && strcmp(line, "    ") != 0) {
			return(OBS_FILE_SKIP_INPUT_LINE);
				   }

		// read phase arrival input
		//CHDAW   P       Q   20.016 1 0.56       10 NWA       0
		//OSS     P       ED  16.524 1 0.11     1672 AHP       0
		//BERNI   P       E   17.930 1 0.18       46 NWA       0

				   istat = ReadFortranString(line, 1, 5, arrival->label);
				   istat += ReadFortranString(line, 9, 6, arrival->phase);
				   istat += ReadFortranString(line, 17, 1, arrival->onset);
				   istat += ReadFortranString(line, 18, 1, arrival->first_mot);
				   istat += ReadFortranReal(line, 20, 7, &arrival->sec);
//              istat += ReadFortranReal(line, 21, 6, &arrival->sec);
				   istat += ReadFortranInt(line, 28, 1, &eth_use_loc);
				   istat += ReadFortranReal(line, 29, 5, &arrival->period);
				   istat += ReadFortranReal(line, 34, 9, &arrival->amplitude);
		//20040226 S Husen bug fix  istat += ReadFortranString(line, 44,9, arrival->inst);
				   istat += ReadFortranString(line, 44, 4, arrival->inst);
				   istat += ReadFortranInt(line, 54, 1, &eth_use_mag);


				   if (istat != 10) {
					   return(OBS_FILE_END_OF_EVENT);
				   }

				   /* remove blanks/whitespace */
				   removeSpace(arrival->label);
				   removeSpace(arrival->phase);
				   removeSpace(arrival->inst);

				   arrival->year = EventTime.year;
				   arrival->month = EventTime.month;
				   arrival->day = EventTime.day;
				   arrival->hour = EventTime.hour;
				   arrival->min = EventTime.min;

				   /* convert onset to error */
//Uncertainties of phase readings:
//                              I             E            Q
//for Key 'Loca':            < +/- 0.05   < +/- 0.2     > +/- 0.2
//for Key 'Regi' or 'Tele'   < +/- 0.20   < +/- 1.0     > +/- 1.0
				   strcpy(arrival->error_type, "GAU");
				   if (eth_use_loc == 0) {
					   arrival->error = 9999.9;
				   } else if (strcmp(arrival->onset, "I") == 0) {
					   arrival->error = 0.05;
				   } else if (strcmp(arrival->onset, "E") == 0) {
					   arrival->error = 0.2;
				   } else if (strcmp(arrival->onset, "Q") == 0) {
					   arrival->error = 0.2;
				   } else {
					   arrival->error = 9999.9;
				   }
				   arrival->quality = 0;

		// check if should use amplitude
				   if (eth_use_mag != 0)
					   arrival->amplitude = AMPLITUDE_NULL;

				   return(istat);
	}



	else if (strcmp(ftype_obs, "NEIC") == 0)
	{

/* example:
20 JUN 2003  (171)

ot  = 06:19:38.51   +/-   0.17              AMAZONAS, BRAZIL
lat =      -7.513   +/-    3.5
lon =     -71.642   +/-    4.6              MAGNITUDE 7.1 (HRV)
dep =       555.2  (depth phases)

110 km (70 miles) E of Cruzeiro do Sul, Brazil (pop N/A)
335 km (205 miles) ENE of Pucallpa, Peru
410 km (255 miles) SSW of Leticia, Colombia
2730 km (1700 miles) WNW of BRASILIA, Brazil

nph =  194 of 401    se = 0.83        FE=113                      A

error ellipse = ( 65.1,155.1,  0.0;  0.0,  0.0,  0.0;  6.3,  4.0,  0.0)

mb = 6.4 ( 93)  ML = 0.0 (  0)  mblg = 0.0 (  0)  md = 0.0 (  0)  MS = 0.0 (  0)

sta  phase     arrival     res   dist azm    amp  per mag     amp  per mag  sta
SAML iPc     06:21:44.12   2.0    8.5 100                                   SAML
eS      06:23:25.14  103.X
LPAZ eP      06:21:51.01  -0.7    9.4 159 L:3.7+0 .93 6.2X g:3.0+0 .93 5.4X LPAZ
OTAV ePc     06:22:02.21   1.7   10.3 318 g:9.9+0 1.0 6.0X                  OTAV
eS      06:23:47.88  107.X
*/
		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}


		/* read event hypocenter line */
		if (!in_hypocenter_event) {
			//(">>event hypocenter line --- %s\n", line);
			/* read hypocenter time */
			//     20 JUN 2003  (171)
			istat = ReadFortranInt(line, 13, 4, &EventTime.year);
			istat += ReadFortranString(line, 9, 3, cmonth);
			istat += ReadFortranInt(line, 6, 2, &EventTime.day);
			if (istat == 3 && EventTime.year > 0 && EventTime.year < 2199 && EventTime.day > 0 && EventTime.day < 32 && strlen(cmonth) == 3) {
				in_hypocenter_event = 1;
				EventTime.month = Month2Int(cmonth);
				origin_hour = 0;
				// find phs line
				while (strncmp(line, " sta", 4) != 0) {
					// check for and get origin hour
					if (strstr(line, "ot  =") != NULL) {
						ReadFortranInt(line, 12, 2, &origin_hour);
					}
					// read next line
					cstat = fgets(line, MAXLINE_LONG, fp_obs);
					if (cstat == NULL)
						return(OBS_FILE_END_OF_INPUT);
				}
				/* read next line */
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}


		/* read phase arrival input */
		//printf(">>phase arrival line --- %s\n", line);
/*
 SAML iPc     06:21:44.12   2.0    8.5 100                                   SAML
      eS      06:23:25.14  103.X
 LPAZ eP      06:21:51.01  -0.7    9.4 159 L:3.7+0 .93 6.2X g:3.0+0 .93 5.4X LPAZ
 KDAK ePn     09:34:19.19  -1.8   16.7  57 b:1.2+3 .50 6.3                   KDAK
 SVW2 eP      09:34:20.52   2.8   16.3  44                                   SVW2
 PMR  ePn     09:34:54.42   1.4   19.3  46 b:1.3+3 1.1 6.1                   PMR
*/
		istat = ReadFortranString(line, 2, 4, arrival->label);
		if (strncmp(arrival->label, "    ", 4) == 0)
			strcpy(arrival->label, last_label);
		else
			strcpy(last_label, arrival->label);
		TrimString(arrival->label);
		TrimString(last_label);
		istat += ReadFortranString(line, 7, 1, arrival->onset);
		istat += ReadFortranString(line, 8, 6, arrival->phase);
		TrimString(arrival->phase);
		// check for valid onset
		pchr = arrival->onset;
		// onset 'x' added by AJL to allow zero weighting of phases
		if (!(*pchr == ' ' || *pchr == 'i' || *pchr == 'e' || *pchr == 'q' || *pchr == 'x')) {
			sprintf(chrtmp, "%c%s", *pchr, arrival->phase);
			strcpy(arrival->phase, chrtmp);
			*pchr = ARRIVAL_NULL_CHR;
		}
		// check for first motion
		pchr = &(arrival->phase[strlen(arrival->phase) - 1]);
		if (*pchr == 'c' || *pchr == 'd') {
			arrival->first_mot[0] = *pchr;
			*pchr = '\0';
		}
		// set weight, check for P - reset weight
		if (arrival->onset[0] == 'i')
			arrival->quality = 0;
		else if (arrival->onset[0] == 'e')
			arrival->quality = 1;
		else if (arrival->onset[0] == 'x')
			arrival->quality = 4;
		else
			arrival->quality = 2;
		if (strncmp(arrival->phase, "P", 1) != 0) {
			arrival->quality += 1;
		}
		istat += ReadFortranInt(line, 15, 2, &arrival->hour);
		// check for day jump
		//printf("   arrival->hour %d  origin_hour %d\n", arrival->hour, origin_hour);
		if (arrival->hour < origin_hour)
			arrival->hour += 24;
		istat += ReadFortranInt(line, 18, 2, &arrival->min);
		istat += ReadFortranReal(line, 21, 5, &arrival->sec);

		if (istat != 6) {
			return(OBS_FILE_END_OF_EVENT);
		}

		arrival->year = EventTime.year;
		arrival->month = EventTime.month;
		arrival->day = EventTime.day;


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "ISC_IMS1.0") == 0)
	{

/* example:
		...
		DATA_TYPE BULLETIN IMS1.0:short
		ISC Comprehensive Bulletin
		Event  2957613 Sicily

		Date       Time        Err   RMS Latitude Longitude  Smaj  Smin  Az Depth   Err Ndef Nsta Gap  mdist  Mdist Qual   Author      OrigID
		2002/04/05 04:52:15.70         0.60  38.7500   14.2800                  14.0               6                          BJI        4180045
		2002/04/05 04:52:20.23   0.05        37.9440   16.4600  38.9  17.4  -1  10.0   1.1         9 348                      PDG        4681048
		...
		Sta     Dist  EvAz Phase        Time      TRes  Azim AzRes   Slow   SRes Def   SNR       Amp   Per Qual Magnitude    ArrID
		MSI     0.45 121.8 Pg       04:52:30.8     1.0                           T__                        _e            71903810
		SCLL    0.55 110.1 Pg       04:52:32.5     1.0                           T__                        _e            71903811
		MTTG    0.67 131.6 Pg       04:52:34.5     0.6                           T__                        _e            71903812
		...
		SGO     2.12   5.0 PN       04:52:59.0     2.6                           T__                        _e            71903828
		LVI     2.19 258.8 PN       04:52:58.3     0.8                           T__                        _e            71903826
		LVI     2.19 258.8                                                       ___           736.0  0.60  __            71903827
		MRLC    2.33   8.0 PN       04:53:01.3     1.9                           T__                        _e            71903830
		CSSN    2.41 359.2 PN       04:53:01.0     0.4                           T__                        _e            71903829
		...
		AVF    11.99 317.8 P        04:55:12.9   -1.37                           T__            20.8  1.26  _e            71869701
		LOR    12.04 320.6 P        04:55:13.3   -1.73                           T__            22.9  1.39  _e            71869702
		LOR    12.04 320.6 R                                                     ___           126.3 18.75  _e            71869703
		SSF    12.08 319.1 P        04:55:15.3    -0.3                           T__            12.1  1.04  _e            71869705
		BGF    12.11 315.9 P        04:55:14.6   -1.37                           T__            34.9  1.38  _e            71869704
		MOX    12.44 349.8 P        04:55:31.3   10.96                           T__                        _e            75622714
		MOX    12.44 349.8 LR                                                    ___           700.0 16.00  __        3.8 75622715
		MOX    12.44 349.8 LR                                                    ___           300.0 16.00  __        3.8 75622717
		MOX    12.44 349.8 LR                                                    ___           600.0 15.00  __            75622716
		BRG    12.45 356.7 P        04:55:21.5     1.0                           T__             9.7  1.20  __            75622621
		BRG    12.45 356.7          04:55:40.5                                   ___                        _i            75622622
		BRG    12.45 356.7 LR                                                    ___           870.0 13.80  __        3.9 75622623
		BRG    12.45 356.7 LR                                                    ___            60.0 13.80  __        3.8 75622625
		BRG    12.45 356.7 LR                                                    ___           690.0 13.80  __            75622624
		ETSF   12.66 295.5 P        04:55:22.4    -1.0                           T__            31.9  1.41  _e            71869706
		CLL    12.94 354.2 P        04:55:26     -1.05                           T__                        __            79343809
		...


		STOP

*/
		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("1 %s", line);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes Event or STOP at end of event) */
		if ((in_hypocenter_event && strncmp(line, "Event", 5) == 0)
				   || strncmp(line, "STOP", 4) == 0) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
				   }


				   /* not yet in event, find and read event hypocenter line */
				   if (!in_hypocenter_event) {
			// assume may alredy be in "Event" block
			// find date
					   while (strncmp(line, "   Date", 7) != 0) {
				// read next line
						   cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("2 %s", line);
						   if (cstat == NULL)
							   return(OBS_FILE_END_OF_INPUT);
					   }
			// read next line
					   cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("3 %s", line);
					   if (cstat == NULL)
						   return(OBS_FILE_END_OF_INPUT);
					   /* read hypocenter date */
//2002/04/05 04:52:15.70         0.60  38.7500   14.2800                  14.0               6                          BJI        4180045
					   istat = ReadFortranInt(line, 1, 4, &EventTime.year);
					   istat += ReadFortranInt(line, 6, 2, &EventTime.month);
					   istat += ReadFortranInt(line, 9, 2, &EventTime.day);
					   if (istat == 3) {
						   in_hypocenter_event = 1;
				// find phases lines
						   while (strncmp(line, "Sta     Dist", 12) != 0) {
					// read next line
							   cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("4 %s", line);
							   if (cstat == NULL)
								   return(OBS_FILE_END_OF_INPUT);
						   }
						   /* read next line */
						   cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("5 %s", line);
						   if (cstat == NULL)
							   return(OBS_FILE_END_OF_INPUT);
					   } else {
						   return(OBS_FILE_END_OF_EVENT);	// blank line or ignored line
					   }
				   }


				   /* read phase arrival input */
/*
				   LVI     2.19 258.8 PN       04:52:58.3     0.8                           T__                        _e            71903826
				   LVI     2.19 258.8                                                       ___           736.0  0.60  __            71903827
*/

		// check if is a reading with a seconds time
		//while (ReadFortranString(line, 44, 3, isc_time_str) != 1
				   while (ReadFortranString(line, 35, 3, isc_time_str) != 1
								      || strncmp(isc_time_str, "   ", 3) == 0) {
			// not a time, check if end
					   if (cstat == NULL)
						   return(OBS_FILE_END_OF_INPUT);
					   if (strncmp(line, "Event", 5) == 0  || strncmp(line, "STOP", 4) == 0)
						   return(OBS_FILE_END_OF_EVENT);
			// not a time, not end, read next line
					   cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("6 %s", line);
					   if (cstat == NULL)
						   return(OBS_FILE_END_OF_INPUT);
					   if (strncmp(line, "Event", 5) == 0  || strncmp(line, "STOP", 4) == 0)
						   return(OBS_FILE_END_OF_EVENT);
								      }

		// read phase time reading
								      istat = ReadFortranString(line, 1, 5, arrival->label);
								      if (strncmp(arrival->label, "     ", 5) == 0)
									      strcpy(arrival->label, last_label);
								      else
									      strcpy(last_label, arrival->label);
								      TrimString(arrival->label);
								      TrimString(last_label);
								      istat += ReadFortranString(line, 20, 8, arrival->phase);
								      TrimString(arrival->phase);
								      istat += ReadFortranInt(line, 29, 2, &arrival->hour);
								      istat += ReadFortranInt(line, 32, 2, &arrival->min);
								      istat += ReadFortranReal(line, 35, 6, &arrival->sec);
								      istat += ReadFortranString(line, 101, 1, arrival->first_mot);
								      istat += ReadFortranString(line, 102, 1, arrival->onset);
		// set weight, check for P - reset weight
								      if (arrival->onset[0] == 'i')
									      arrival->quality = 0;
								      else if (arrival->onset[0] == 'e')
									      arrival->quality = 1;
								      else if (arrival->onset[0] == 'q')
									      arrival->quality = 2;
								      else if (arrival->onset[0] == '_')
									      arrival->quality = 2;
								      else
									      arrival->quality = 3;
		// check if "P" arrival
								      if (strncmp(arrival->phase, "P", 1) != 0) {
									      arrival->quality += 1;
								      }

								      if (istat != 7) {
									      return(OBS_FILE_INVALID_PHASE);
								      }

								      arrival->year = EventTime.year;
								      arrival->month = EventTime.month;
								      arrival->day = EventTime.day;


								      /* convert quality to error */
								      Qual2Err(arrival);

								      return(istat);
	}



	else if (strcmp(ftype_obs, "CSEM_GSE2.0") == 0)
	{

/* example:
		...
		BEGIN GSE2.0
		MSG_TYPE DATA
		MSG_ID 041203222858 EMSC
		DATA_TYPE BULLETIN GSE2.0

		EVENT 20041203222858
		Date       Time       Latitude Longitude    Depth    Ndef Nsta Gap    Mag1  N    Mag2  N    Mag3  N  Author          ID
		rms   OT_Error      Smajor Sminor Az        Err   mdist  Mdist     Err        Err        Err     Quality

		2004/12/03 22:28:58.5     44.2742    7.5419      8.0      66   53 109  ML 3.3 11                        MIX       00000001
		1.22   +-  0.10       2.6    1.6  123               0.42   7.13   +-0.6                           a i ke

		( Data from stations operated by :     LDG,   MDD,   ZUR )

		NORTHERN ITALY
		Sta     Dist  EvAz     Phase      Date       Time     TRes  Azim  AzRes  Slow  SRes Def   SNR       Amp   Per   Mag1   Mag2       ID
		SBF     0.42 190.6 a   Pn      2004/12/03 22:29:09.3   1.3                          T     1.0                                    LDG
		MBDF    0.71 309.7 a   Pg      2004/12/03 22:29:09.8  -2.4                          T     1.0                                    LDG
		FRF     0.96 222.5 a   Pg      2004/12/03 22:29:16.4  -0.2                          T     1.0                                    LDG
		FRF     0.96 222.5 a   Sg      2004/12/03 22:29:27.9  -1.3                          T     1.0     134.3  0.16 ML 4.0             LDG
		...
		BAIF    6.22 339.7 a   Pn      2004/12/03 22:30:30.2   1.9                          T     1.0                                    LDG
		ERTA    6.27 240.5 m   P       2004/12/03 22:30:31.9  -1.8                          T               0.8  0.16                    MAD
		LDF     6.83 311.9 a   Pn      2004/12/03 22:30:38.8   2.1                          T     1.0                                    LDG
		GRR     7.12 308.2 a   Pn      2004/12/03 22:30:41.8   1.1                          T     1.0                                    LDG
		FLN     7.13 311.9 a   Pn      2004/12/03 22:30:41.8   1.1                          T     1.0                                    LDG

		STOP

		...

*/
		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("1 %s", line);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes Event or STOP at end of event) */
		if ((in_hypocenter_event && strncmp(line, "EVENT", 5) == 0)
				   || strncmp(line, "STOP", 4) == 0) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}


		/* not yet in event, find and read event hypocenter line */
		if (!in_hypocenter_event) {
			// assume may alredy be in "Event" block
			// find phases lines
			// following works for CSEM:
			//while (strncmp(line, "Sta     Dist", 12) != 0) {
			// does not work for INGV which has: "Sta    Dist"
			while (strncmp(line, "Sta    ", 7) != 0) {
				// read next line
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("4 %s", line);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			}
			// read next line
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("5 %s", line);
			if (cstat == NULL)
				return(OBS_FILE_END_OF_INPUT);
			in_hypocenter_event = 1;
		}


		// read phase arrival input



/* this block needed for ISC_IMSL because of amplitude readings, etc.
// check if is a reading with a seconds time
//while (ReadFortranString(line, 44, 3, isc_time_str) != 1
		while (ReadFortranString(line, 35, 3, isc_time_str) != 1
		|| strncmp(isc_time_str, "   ", 3) == 0) {
// not a time, check if end
		if (cstat == NULL || strncmp(line, "Event", 5) == 0
		|| strncmp(line, "STOP", 4) == 0)
		return(OBS_FILE_END_OF_EVENT);
// not a time, not end, read next line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
//printf("6 %s", line);
		if (cstat == NULL)
		return(OBS_FILE_END_OF_INPUT);
}
*/

		// read phase time reading
		istat = ReadFortranString(line, 1, 5, arrival->label);
		if (strncmp(arrival->label, "     ", 5) == 0)
			strcpy(arrival->label, last_label);
		else
			strcpy(last_label, arrival->label);
		TrimString(arrival->label);
		TrimString(last_label);
		istat += ReadFortranString(line, 24, 8, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranInt(line, 32, 4, &arrival->year);
		istat += ReadFortranInt(line, 37, 2, &arrival->month);
		istat += ReadFortranInt(line, 40, 2, &arrival->day);
		istat += ReadFortranInt(line, 43, 2, &arrival->hour);
		istat += ReadFortranInt(line, 46, 2, &arrival->min);
		istat += ReadFortranReal(line, 49, 5, &arrival->sec);
		istat += ReadFortranString(line, 21, 1, arrival->first_mot);
		istat += ReadFortranString(line, 22, 1, arrival->onset);
		// set weight, check for P - reset weight
		if (arrival->onset[0] == 'i')
			arrival->quality = 0;
		else if (arrival->onset[0] == ' ')
			arrival->quality = 0;
		else if (arrival->onset[0] == 'e')
			arrival->quality = 1;
		else if (arrival->onset[0] == 'q')
			arrival->quality = 2;
		else if (arrival->onset[0] == '_')
			arrival->quality = 2;
		else
			arrival->quality = 3;
		// check if "P" arrival
		if (strncmp(arrival->phase, "P", 1) != 0) {
			arrival->quality += 1;
		}

		if (istat != 10) {
			return(OBS_FILE_INVALID_PHASE);
		}

		// convert quality to error
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "CSEM_ALERT") == 0)
	{

/* example:

		...
		Sta       Phase     Date     Time       Res     Dist Azm    Net
		---------------------------------------------------------------
		PRNI  m   Pn    2004/02/11 08:15:27.7   0.4     1.41 197    GII
		PRNI  m   S     2004/02/11 08:15:46.6   1.5     1.41 197    GII
		KSDI  m   Pn    2004/02/11 08:15:29.9   1.6     1.49   5    GII
		KSDI  a   P     2004/02/11 08:15:28.8   0.3     1.49   5    ODC
		MAMC  ad  P     2004/02/11 08:16:03.9   0.4     3.95 331    CYP
		UZH   m i P     2004/02/11 08:19:33.0   1.1    19.64 333    LVV
*/
		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}


		if (!in_hypocenter_event) {
			// find phase line
			while (strncmp(line, "Sta       Phase", 15) != 0) {
				// read next line
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			}
			// skip next line */
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			if (cstat == NULL)
				return(OBS_FILE_END_OF_INPUT);
			in_hypocenter_event = 1;
		}


		/* read phase arrival input */
/*
		PRNI  m   Pn    2004/02/11 08:15:27.7   0.4     1.41 197    GII
		PRNI  m   S     2004/02/11 08:15:46.6   1.5     1.41 197    GII
		KSDI  m   Pn    2004/02/11 08:15:29.9   1.6     1.49   5    GII
		KSDI  a   P     2004/02/11 08:15:28.8   0.3     1.49   5    ODC
		MAMC  ad  P     2004/02/11 08:16:03.9   0.4     3.95 331    CYP
		UZH   m i P     2004/02/11 08:19:33.0   1.1    19.64 333    LVV
*/
		istat = ReadFortranString(line, 1, 4, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 11, 5, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranString(line, 8, 1, arrival->first_mot);
		if (arrival->first_mot[0] == ' ')
			arrival->first_mot[0] = ARRIVAL_NULL_CHR;
		istat += ReadFortranString(line, 9, 1, arrival->onset);
		if (arrival->onset[0] == ' ')
			arrival->onset[0] = ARRIVAL_NULL_CHR;
		// set weight, check for P - reset weight
		arrival->quality = arrival->onset[0] == 'i' ? 0 : 1;
		if (strncmp(arrival->phase, "P", 1) != 0) {
			arrival->quality += 1;
		}
		istat += ReadFortranInt(line, 17, 4, &arrival->year);
		istat += ReadFortranInt(line, 22, 2, &arrival->month);
		istat += ReadFortranInt(line, 25, 2, &arrival->day);
		istat += ReadFortranInt(line, 28, 2, &arrival->hour);
		istat += ReadFortranInt(line, 31, 2, &arrival->min);
		istat += ReadFortranReal(line, 34, 5, &arrival->sec);

		if (istat != 10) {
			return(OBS_FILE_END_OF_EVENT);
		}


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "INGV_AUTOPICKS") == 0)
	{

/* example:
		...
		FUORNHHE 2005/04/30 23:22:15.29
		FUORNHHZ 2005/04/30 23:23:50.97
		FUORNHHE 2005/04/30 23:27:59.99
		FUORNHHE 2005/04/30 23:27:59.99  4715 63.21 238.72
		TOLF HHE 2005/04/30 23:38:48.56
		TOLF HHE 2005/04/30 23:38:48.56  1770 0.06 26.75
		...
*/

		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}

		/* read phase arrival input */
		istat = ReadFortranString(line, 1, 5, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 6, 2, arrival->inst);
		istat += ReadFortranString(line, 8, 1, arrival->comp);
		strcpy(arrival->phase, "P");
		arrival->quality = 0;
		istat += ReadFortranInt(line, 10, 4, &arrival->year);
		istat += ReadFortranInt(line, 15, 2, &arrival->month);
		istat += ReadFortranInt(line, 18, 2, &arrival->day);
		istat += ReadFortranInt(line, 21, 2, &arrival->hour);
		istat += ReadFortranInt(line, 24, 2, &arrival->min);
		istat += ReadFortranReal(line, 27, 5, &arrival->sec);

		if (istat != 9) {
			return(OBS_FILE_END_OF_EVENT);
		}


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "NCEDC_UCB") == 0)
	{

/* example:
		...
		19990818 010619.0000 +37.8950 -122.7000 010.0000 0.00 000 4.87 008 4.99 040 0.00 000 0.000e+00 000 099 180 009.47 00.0300 00.2000 00.2000 00.4000 00.1700 x F
		$COM Bolinas, 19 km WSW of San Rafael, CA
		$COM BRK: Mo=1.23E+23 dyne-cm, Mw=4.7.  Minor damage reported in Bolinas and San Rafael; felt from Santa Rosa in the north to San Jose in the
		$COM south.
		$AMP CMSB CL   2 WAS  407.40 001.10 0.00e+00
		$AMP CMSB CL   3 WAS  596.20 000.77 0.00e+00
		$PHS CMSB CL   1 i P        d 19990818 010625.9300 0039.535 093.58 000.00 000.00 0.00e+00 Y
		$PHS CMSB CL   3 e S        x 19990818 010630.8700 0039.535 093.58 000.00 000.00 0.00e+00 N
		$AMP CRQB CL   2 WAS  972.90 000.76 0.00e+00
		$AMP CRQB CL   3 WAS  1122.00 000.82 0.00e+00
		$PHS CRQB CL   1 i P        d 19990818 010627.5600 0045.409 066.72 000.00 000.00 0.00e+00 N
		$PHS CRQB CL   3 i S        x 19990818 010633.6500 0045.409 066.72 000.00 000.00 0.00e+00 N
*/
		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL )
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)  || strncmp(line, "$END", 4) == 0) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}


		// find next phase line
		while (strncmp(line, "$PHS", 4) != 0) {
			// read next line
			cstat = fgets(line, MAXLINE_LONG, fp_obs);
			if (cstat == NULL || strncmp(line, "$END", 4) == 0)
				return(OBS_FILE_END_OF_EVENT);
		}


		/* read phase arrival input */
/* example:
		$PHS CRQB CL   1 i P        d 19990818 010627.5600 0045.409 066.72 000.00 000.00 0.00e+00 N
		$PHS CRQB CL   3 i S        x 19990818 010633.6500 0045.409 066.72 000.00 000.00 0.00e+00 N
		$PHS MHC  VSP  Z i P        c 19990818 010636.8500 0111.833 123.00 000.00 000.00 0.00e+00 N
		$PHS MHC  VSP  E i S        x 19990818 010651.9000 0111.833 123.00 000.00 000.00 0.00e+00 N
*/
/*
		$PHS ZSP  TS13 Z i P        d 19890212 195041.9000 0032.674 068.58 000.00 000.00 0.00e+00 N
		$PHS ZSP  TS13 Z i S        x 19890212 195047.4000 0032.674 068.58 000.00 000.00 0.00e+00 N
		$PHS NFI  x    Z 0 P        c 19890212 195042.2500 0038.265 246.22 000.00 000.00 0.00e+00 Y
		$PHS JEG  x    Z 0 P        d 19890212 195042.4500 0038.037 161.07 000.00 000.00 0.00e+00 N
		$PHS PCC  B14  Z i P        c 19890212 195042.8000 0232.545 146.11 000.00 000.00 0.00e+00 Y
		$PHS PCC  B14  Z i S        x 19890212 195048.7000 0232.545 146.11 000.00 000.00 0.00e+00 N
		$PHS NPR  x    Z 3 P        c 19890212 195044.1300 0040.492 295.89 000.00 000.00 0.00e+00 N
*/
		istat = ReadFortranString(line, 6, 4, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 11, 4, arrival->inst);
		TrimString(arrival->inst);
		istat += ReadFortranString(line, 16, 1, arrival->comp);
		TrimString(arrival->comp);
		istat += ReadFortranString(line, 18, 1, arrival->onset);
		if (arrival->onset[0] == '?')
			arrival->onset[0] = ARRIVAL_NULL_CHR;
		istat += ReadFortranString(line, 20, 8, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranString(line, 29, 1, arrival->first_mot);
		if (arrival->first_mot[0] == 'x')
			arrival->first_mot[0] = ARRIVAL_NULL_CHR;
		// set weight, check for P - reset weight
		if (arrival->onset[0] == 'x' || arrival->onset[0] == 'X')
			arrival->quality = 1;
		if (arrival->onset[0] == 'i')
			arrival->quality = 0;
		if (arrival->onset[0] == 'e')
			arrival->quality = 1;
		if (arrival->onset[0] == '?')
			arrival->quality = 1;
		if (arrival->onset[0] == '0')
			arrival->quality = 0;
		if (arrival->onset[0] == '1')
			arrival->quality = 1;
		if (arrival->onset[0] == '2')
			arrival->quality = 2;
		if (arrival->onset[0] == '3')
			arrival->quality = 3;
		if (arrival->onset[0] == '4')
			arrival->quality = 4;
		if (strncmp(arrival->phase, "P", 1) != 0) {
			arrival->quality += 1;
		}
		istat += ReadFortranInt(line, 31, 4, &arrival->year);
		istat += ReadFortranInt(line, 35, 2, &arrival->month);
		istat += ReadFortranInt(line, 37, 2, &arrival->day);
		istat += ReadFortranInt(line, 40, 2, &arrival->hour);
		istat += ReadFortranInt(line, 42, 2, &arrival->min);
		istat += ReadFortranReal(line, 44, 7, &arrival->sec);

		if (istat != 12) {
			return(OBS_FILE_END_OF_EVENT);
		}


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "HYPOCENTER") == 0)
	{

/* example:
		67  827 1632 12.4 L
		ZAK0SZ  PG      1632  33.30
		ZAK0SZ  SG      1632  50.80
		IRK0SZ  PG      1632  29.50
		IRK0SZ  SG      1632  43.90
		KHT0SZ  PG      1632  34.00
*/

		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}


		/* read event hypocenter line */
		if (!in_hypocenter_event) {
			/* read hypocenter time */
			istat = ReadFortranInt(line, 1, 5, &EventTime.year);
			istat += ReadFortranInt(line, 7, 2, &EventTime.month);
			istat += ReadFortranInt(line, 9, 2, &EventTime.day);
			if (istat == 3) {
				if (EventTime.year < 2000)
					EventTime.year += 1900;
				/* !! assume any 2 digit year is 1900-1999 */
				in_hypocenter_event = 1;
				/* read next line */
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}


		/* read phase arrival input */

		istat = ReadFortranString(line, 3, 3, arrival->label);
		istat += ReadFortranInt(line, 6, 1, &arrival->quality);	// !!! is this field quality???
		istat += ReadFortranString(line, 7, 2, arrival->comp);
		istat += ReadFortranString(line, 11, 6, arrival->phase);
		istat += ReadFortranInt(line, 19, 2, &arrival->hour);
		istat += ReadFortranInt(line, 21, 2, &arrival->min);
		// format of seconds varies, so cannot use fixed fortran read
		//istat += ReadFortranReal(line, 25, 5, &arrival->sec);
		istat += sscanf(line, "%*s %*s %*s %lf", &arrival->sec);

		if (istat != 7) {
			return(OBS_FILE_END_OF_EVENT);
		}


		arrival->year = EventTime.year;
		arrival->month = EventTime.month;
		arrival->day = EventTime.day;


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}



	else if (strcmp(ftype_obs, "HYPODD_PHA") == 0)
	{

/* example:
# 1985  1 24  2 19 58.71  37.8832 -122.2415    9.80 1.40  0.15  0.51  0.02      38542
		NCCSP       2.850  -1.000   P
		NCCBW       3.430  -1.000   P
		NCCMC       2.920   0.200   P
		NCCAI       3.440  -1.000   P
		NCCBR       3.940   1.000   P
		NCCLC       4.610   0.500   P
		NCJPR       4.470   0.100   P
		NCNLH       6.000   0.200   P
		NCCSH       6.130   0.100   P
		NCNTA       5.630   0.100   P
		NCJMG       6.240   0.200   P
*/

		if (in_hypocenter_event) {
			/* check for end of event (assumes no blanks after last phase) */
			chr = fgetc(fp_obs);
			if (chr == EOF) {
				return(OBS_FILE_END_OF_INPUT);
			} else if (chr == '#') {
				// end of event
				ungetc(chr, fp_obs);
				return(OBS_FILE_END_OF_EVENT);
			} else {
				ungetc(chr, fp_obs);
			}
		}

		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);


		/* read event hypocenter line */
		if (!in_hypocenter_event) {
			/* read hypocenter time */
			istat = sscanf(line, "# %d %d %d %d %d %lf",
				       &EventTime.year, &EventTime.month, &EventTime.day,
				       &EventTime.hour, &EventTime.min, &EventTime.sec);
			if (istat == 6) {
				in_hypocenter_event = 1;
				/* read next line */
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			} else {
				return(OBS_FILE_END_OF_EVENT);
			}
		}


		// read phase arrival input

		istat = sscanf(line, "%s %lf %lf %s", chrtmp, &ttime, &weight, arrival->phase);
		if (istat != 4)
			return(OBS_FILE_END_OF_EVENT);

		// trim label (to agree with Alberto's 3 char convention)
//		if (strlen(chrtmp) > 4 && strncmp(chrtmp, "NC", 2) == 0)
//			strcpy(arrival->label, chrtmp + 2);
//		else
		strcpy(arrival->label, chrtmp);

		arrival->hour = EventTime.hour;
		arrival->min = EventTime.min;
		arrival->sec = EventTime.sec + ttime;

		arrival->year = EventTime.year;
		arrival->month = EventTime.month;
		arrival->day = EventTime.day;

		// convert weight to quality (following Appendix C in hypoDD.pdf)
		weight = fabs(weight);
		if (weight > 0.99)
			arrival->quality = 0;
		else if (weight > 0.49)
			arrival->quality = 1;
		else if (weight > 0.19)
			arrival->quality = 2;
		else if (weight > 0.09)
			arrival->quality = 3;
		else
			arrival->quality = 4;
		// convert quality to error
		Qual2Err(arrival);

		return(istat);
	}


	// DD
	else if (strcmp(ftype_obs, "HYPODD_CC") == 0 || strcmp(ftype_obs, "HYPODD_CT") == 0
			|| strcmp(ftype_obs, "HYPODD_") == 0)
	{

/* HYPODD_CC example:
#    28136    46442     -0.174000
		NCCMO    -0.038245201    0.63    P
		NCCRP    -0.062200069    0.78    P
		NCJBG    -0.020649910    0.60    P
		NCJPS    -0.011000156    0.50    P
*/
/* OTC - Origin time correction relative to the event origin time reported
		in the catalog data. If not known for all available data,
		then cross correlation data can not be used in combination
		with catalog data. Set OTC to -999 if not known for individual
		observations; Set to 0.0 if cross-correlation and catalog origin
		times are identical, or if only cross-correlation data is used.
*/
/* HYPODD_CT example:
#     38542     38520
		NCCBW     3.430   3.430 1.0000 P
		NCCSP     2.850   2.840 1.0000 P
		NCNLN     8.950   8.950 1.0000 P
		NCCAI     3.440   3.420 1.0000 P
*/


		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);


		/* read events line */
		if (line[0] == '#') {
			hypo_cc_flag = 1;
			istat = sscanf(line, "# %ld %ld %lf",
				       &dd_event_id_1, &dd_event_id_2, &dd_otime_corr);
			if (dd_event_id_1 == dd_event_id_2)
				printf("ERROR: dd_event_id_1 == &dd_event_id_2  %ld %ld\n", dd_event_id_1, dd_event_id_2);
			if (istat == 2)
				hypo_cc_flag = 0;

			if (istat >= 2) {
				in_hypocenter_event = 1;
				/* read next line */
				cstat = fgets(line, MAXLINE_LONG, fp_obs);
				if (cstat == NULL)
					return(OBS_FILE_END_OF_INPUT);
			} else {
				printf("ERROR: format error: %s\n", line);
				return(OBS_FILE_FORMAT_ERROR);
			}
		}


		// read phase arrival input

		arrival->xcorr_flag = hypo_cc_flag;

		if (hypo_cc_flag) {	// HYPODD_CC

			istat = sscanf(line, "%s %lf %lf %s",
				       arrival->label, &arrival->dd_dtime, &arrival->weight, arrival->phase);
	//printf("%s %lf %lf %s\n", arrival->label, arrival->dd_dtime, arrival->weight, arrival->phase);
			if (istat != 4) {
				printf("ERROR: HYPODD_CC format error: %s\n", line);
				return(OBS_FILE_FORMAT_ERROR);
			}

			arrival->dd_event_id_1 = dd_event_id_1;
			arrival->dd_event_id_2 = dd_event_id_2;
			// incorporate OTC into dd_dtime when reading dt file
			arrival->dd_dtime -= dd_otime_corr;
			strcpy(arrival->error_type, "XCC");

		} else {	// HYPODD_CT

			istat = sscanf(line, "%s %lf %lf %lf %s",
				       arrival->label, &tt_sta1, &tt_sta2, &arrival->weight, arrival->phase);
	//printf("%s %lf %lf %lf %s\n", arrival->label, tt_sta1, tt_sta2, arrival->weight, arrival->phase);
			if (istat != 5) {
				printf("ERROR: HYPODD_CT format error: %s\n", line);
				return(OBS_FILE_FORMAT_ERROR);
			}

			arrival->dd_event_id_1 = dd_event_id_1;
			arrival->dd_event_id_2 = dd_event_id_2;
			arrival->dd_dtime = tt_sta1 - tt_sta2;
			strcpy(arrival->error_type, "CAT");

		}

// add noise
//arrival->dd_dtime += 0.02 * get_rand_double(-1.0, 1.0);
		//

		return(istat);
	}



	else if (strcmp(ftype_obs, "RENASS_WWW") == 0)
	{

		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);


		/* read formatted P arrival input */
		istat = sscanf(line, "%s %s %dh %dmn %lfsec",
			       arrival->label, arrival->phase, &arrival->hour,
			       &arrival->min, &arrival->sec);

		if (istat != 5) {
			line[0] = '\0';
			return(OBS_FILE_SKIP_INPUT_LINE);
		}

		arrival->quality = 0;
		arrival->year = 1900;
		arrival->month = 01;
		arrival->day = 01;


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}

	else if (strcmp(ftype_obs, "RENASS_DEP") == 0)
	{

		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);


		/* read formatted arrival input */
		istat = ReadFortranString(line, 1, 4, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 20, 4, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranInt(line, 25, 2, &arrival->hour);
		istat += ReadFortranInt(line, 28, 2, &arrival->min);
		istat += ReadFortranReal(line, 30, 5, &arrival->sec);

		if (istat != 5) {
			line[0] = '\0';
			return(OBS_FILE_SKIP_INPUT_LINE);
		}


		arrival->quality = 0;
		arrival->year = EventTime.year;
		arrival->month = EventTime.month;
		arrival->day = EventTime.day;
		if (arrival->hour != EventTime.hour)
			puterr("WARNING: filename and arrival hours do not match.");
		if (arrival->min != EventTime.min)
			puterr("WARNING: filename and arrival minutes do not match.");


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);
	}

	else if (strcmp(ftype_obs, "SEISAN") == 0 ||
			strcmp(ftype_obs, "NORDIC") == 0)
	{
		/* read next line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);

		/* check for blank line */
		if (LineIsBlank(line))
			return(OBS_FILE_END_OF_EVENT);



		// check for date info
		if (!date_saved) {
			/* Check whether it is a hypocenter line */
			if (line[79]!='1') {
				puterr2("ERROR: bad SEISAN hypocentre line",line);
				return(OBS_FILE_FORMAT_ERROR);
			}
			istat = ReadFortranInt(line, 2, 4, &year_save);
			/* in SEISAN data the year is always fully specified. The correction below is susceptiple to millenium bug */
			/* if (year_save < 500) */
			/* 	year_save += 1900; */
			istat += ReadFortranInt(line, 7, 2, &month_save);
			istat += ReadFortranInt(line, 9, 2, &day_save);
/*printf(
			"SEISAN_DATE Read: istat %d   -  %d %d %d\n",
			istat, year_save, month_save, day_save);*/
			if (istat == 3 && IsGoodDate(year_save,
			    month_save, day_save)) {
				    date_saved = 1;
				    return(OBS_FILE_SKIP_INPUT_LINE);
			    }
		}


		/* ignore all lines which are not phase observation lines */
		if (line[79]!=' ' && line[79]!='4')
			return(OBS_FILE_SKIP_INPUT_LINE);

		/* read formatted P arrival input */
		istat = ReadFortranString(line, 2, 5, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 7, 1, arrival->inst);
		TrimString(arrival->inst);
		istat += ReadFortranString(line, 8, 1, arrival->comp);
		TrimString(arrival->comp);
		istat += ReadFortranString(line, 10, 1, arrival->onset);
		TrimString(arrival->onset);
		istat += ReadFortranString(line, 11, 4, arrival->phase);
		TrimString(arrival->phase);
		istat += ReadFortranInt(line, 15, 1, &arrival->quality);
		istat += ReadFortranString(line, 17, 1, arrival->first_mot);
		TrimString(arrival->first_mot);
		istat += ReadFortranInt(line, 19, 2, &arrival->hour);
		istat += ReadFortranInt(line, 21, 2, &arrival->min);
		istat += ReadFortranReal(line, 23, 6, &arrival->sec);
		istat += ReadFortranReal(line, 30, 4, &arrival->coda_dur);
		istat += ReadFortranReal(line, 34, 7, &arrival->amplitude);
		istat += ReadFortranReal(line, 42, 4, &arrival->period);
		if (date_saved) {
			arrival->year = year_save;
			arrival->month = month_save;
			arrival->day = day_save;
		} else {
			arrival->year = 1900;
			arrival->month = 01;
			arrival->day = 01;
		}

		/*printf(
		"SEISAN Read: istat %d   -  %s %s %s %d %s %d %d %d %d %d %lf %lf\n",
		istat, arrival->label, arrival->onset, arrival->phase, arrival->quality, arrival->first_mot, arrival->year, arrival->month, arrival->day, arrival->hour, arrival->min, arrival->sec, arrival->coda_dur);*/

		if (istat != 13) {
			puterr2("ERROR: bad SEISAN phase line:",line);
			return(OBS_FILE_FORMAT_ERROR);
		}

		/* convert quality to error */
		Qual2Err(arrival);



		return(istat);
	}


	else if (strcmp(ftype_obs, "SAFOD") == 0)
	{

/* example:

	Location Header
	arrival 1
	arrival 2
	...
	arrival n
		(blank line signifying end of phases for this event)

Here's an example:

1994 259  3  0   0.0000   0.46899    1.72781  -0.7000      994Shot01
MMCR  1994 259  3  0   0.330 P  -0.020   0.020
MMEC  1994 259  3  0   0.720 P  -0.724   0.724
MMEN  1994 259  3  0   0.950 P  -0.020   0.020


The Header:
The fields in the location header are: year, julian day, hour, minute, second, local x, local y, local z, event ID.
The time fields give the current estimate of the origin time, the x,y,z coordinates are in a reference fram local to SAFOD. I will be providing a program to allow you to go between this system and UTM NAD 27 soon.

The Phases:
The fields in the phase line are: station code, year, julian day, hour, minute, second, phase designator (P or S), left uncertainty, right uncertainty.

Often, the left uncertainty field is blank. Any information in colums to the right of the right uncertainty is superfluous (sometimes there will be a residual from a location run, for example).
*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}

		// read phase arrival input
		istat = ReadFortranString(line, 1, 5, arrival->label);
		istat += ReadFortranInt(line, 7, 4, &arrival->year);
		istat += ReadFortranInt(line, 12, 3, &yearday);
		istat += ReadFortranInt(line, 16, 2, &arrival->hour);
		istat += ReadFortranInt(line, 19, 2, &arrival->min);
		istat += ReadFortranReal(line, 22, 7, &arrival->sec);
		istat += ReadFortranString(line, 30, 1, arrival->phase);
		istat += ReadFortranReal(line, 32, 7, &left_uncertainty);
		istat += ReadFortranReal(line, 40, 7, &right_uncertainty);

		TrimString(arrival->label);
		MonthDay(arrival->year, yearday, &arrival->month, &arrival->day);
		if (strcmp(arrival->phase, "0") == 0)
			strcpy(arrival->phase, "P");

		if (istat != 9) {
			if (nfirst) 	// may be header line
				return(OBS_FILE_SKIP_INPUT_LINE);
			puterr2("ERROR: bad SAFOD phase line:",line);
			return(OBS_FILE_END_OF_EVENT);
		}

		// set error fields
		strcpy(arrival->error_type, "GAU");
		arrival->error = right_uncertainty;
		/*
if (strcmp(arrival->phase, "P") == 0)
	arrival->error = 0.1;
if (strcmp(arrival->phase, "S") == 0)
	arrival->error = 0.2;
		*/

		return(istat);

	}


	else if (strcmp(ftype_obs, "INERIS") == 0)
	{

/* example:

#      051013_07425894-TG13  864088 194914 96    -9999
TGe3D   P  2005 10 13 07 42  2.718E-01  0.005
TGe1DB  P  2005 10 13 07 42  2.773E-01  0.005
TGe1DS  P  2005 10 13 07 42  2.786E-01  0.005
TLi3D   P  2005 10 13 07 42  1.766E-01  0.005
TLi1DB  P  2005 10 13 07 42  1.828E-01  0.005

*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}

		// read phase arrival input
		istat = sscanf(line, "%s %s %d %d %d %d %d %lf %lf",
			arrival->label, arrival->phase, &arrival->year, &arrival->month, &arrival->day,
			&arrival->hour, &arrival->min, &arrival->sec, &arrival->error);

		if (istat != 9) {
			if (nfirst) 	// may be header line
				return(OBS_FILE_SKIP_INPUT_LINE);
			puterr2("ERROR: bad SAFOD phase line:",line);
			return(OBS_FILE_END_OF_EVENT);
		}

		// set error fields
		strcpy(arrival->error_type, "GAU");

		return(istat);

	}


	else if (strcmp(ftype_obs, "EW_PKFILTER") == 0)
	{

/* example:

Pass: 10  6 73 4771 FG4  IVSHZ U3  20070323133511.42      33      37      27
Pass: 10  6 73 4772 LAV9 IVSHZ D2  20070323133517.18      50      20      23
Pass: 10  6 73 4773 LAV9 IVSHZ D3  20070323133533.68      35      43      44
Pass: 10  6 73 4774 MSAG IVHHZ  2  20070323133538.59     376     465     346
Pass: 10  6 73 4775 MSC  OVV   D3  20070323133540.00      62     202     211
Pass: 10  6 73 4776 MSC  OVV    1  20070323133549.24     482    1754     270
Pass: 10  6 73 4777 NRCA IVSHZ D0  20070323133542.24     783     259     638
Pass: 10  6 73 4778 BDI  IVHHZ  2  20070323133544.54     575     265      93
Pass: 10  6 73 4779 ARVD IVEHZ  3  20070323133553.28      94      45     135
Pass: 10  6 73 4780 AOI  IVBHZ U2  20070323133554.52     187     154     283
Pass: 10  6 73 4781 SNTG IVSHZ  2  20070323133549.12      40      80      69

*/

		// read line
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		// check for end of event (assumes empty lines between event)
		if (LineIsBlank(line)) {
			// end of event
			return(OBS_FILE_END_OF_EVENT);
		}

		// read phase arrival input
		istat = ReadFortranString(line, 21, 5, arrival->label);
		istat += ReadFortranString(line, 26, 2, arrival->network);
		istat += ReadFortranString(line, 28, 3, arrival->comp);
		istat += ReadFortranString(line, 32, 1, arrival->first_mot);
		istat += ReadFortranInt(line, 33, 1, &arrival->quality);
		istat += ReadFortranInt(line, 36, 4, &arrival->year);
		istat += ReadFortranInt(line, 40, 2, &arrival->month);
		istat += ReadFortranInt(line, 42, 2, &arrival->day);
		istat += ReadFortranInt(line, 44, 2, &arrival->hour);
		istat += ReadFortranInt(line, 46, 2, &arrival->min);
		istat += ReadFortranReal(line, 48, 6, &arrival->sec);

		if (istat != 11) {
			puterr2("ERROR: bad EW_PKFILTER phase line:",line);
			return(OBS_FILE_END_OF_EVENT);
		}

		TrimString(arrival->label);
		TrimString(arrival->network);
		TrimString(arrival->comp);

		strcpy(arrival->phase, "P");

		// convert quality to error
		Qual2Err(arrival);
		// set error fields
		strcpy(arrival->error_type, "GAU");



		return(istat);

	}





	else if (strcmp(ftype_obs, "GEOFON") == 0)
	{

/* example:
  Northern Sumatra, Indonesia  M=6.3  2008/01/22  17:14:57.4  1.10 N  97.49 E   31 km

  Stat  Net   Date       Time          Amp    Per   Res  Dist  Az mb  ML  mB
  GSI   GE  08/01/22  17:15:04.5         0.0  0.0  -0.6   0.2  22 0.0 5.6 0.0
  PPI   IA  08/01/22  17:15:46.5         0.0  0.0  -0.5   3.3 118 0.0 6.1 0.0
  PDSI  IA  08/01/22  17:15:48.8         0.0  0.0  -2.2   3.6 124 0.0 6.2 0.0
  IPM   MY  08/01/22  17:16:10.5         0.0  0.0   1.8   4.9  46 0.0 6.6 0.0
  BSI   IA  08/01/22  17:16:06.1         0.0  0.0  -2.8   4.9 333 0.0 6.4 0.0
  RGRI  IA  08/01/22  17:16:11.7      1867.5  0.8   0.5   5.1 106 6.1 6.5 0.0
  KUM   MY  08/01/22  17:16:14.4      2527.4  1.1   0.9   5.2  37 6.1 6.5 0.0
  NTU   MS  08/01/22  17:16:27.4        34.5  0.9   0.6   6.2  88 4.5 5.6 0.0
  KOM   MY  08/01/22  17:16:30.4       549.7  1.3   0.9   6.4  84 5.5 6.1 6.3
		...
*/

		/* read line */
		cstat = fgets(line, MAXLINE_LONG, fp_obs);
		if (cstat == NULL)
			return(OBS_FILE_END_OF_INPUT);
		/* check for end of event (assumes empty lines between event) */
		if (LineIsBlank(line)) {
			/* end of event */
			return(OBS_FILE_END_OF_EVENT);
		}

		/* read phase arrival input */
		istat = ReadFortranString(line, 3, 5, arrival->label);
		TrimString(arrival->label);
		istat += ReadFortranString(line, 9, 2, arrival->network);
		TrimString(arrival->network);
		strcpy(arrival->phase, "P");
		arrival->quality = 0;
		istat += ReadFortranInt(line, 13, 2, &arrival->year);
		arrival->year += 2000;
		istat += ReadFortranInt(line, 16, 2, &arrival->month);
		istat += ReadFortranInt(line, 19, 2, &arrival->day);
		istat += ReadFortranInt(line, 23, 2, &arrival->hour);
		istat += ReadFortranInt(line, 26, 2, &arrival->min);
		istat += ReadFortranReal(line, 29, 5, &arrival->sec);

		if (istat != 8) {
			return(OBS_FILE_END_OF_EVENT);
		}


		/* convert quality to error */
		Qual2Err(arrival);

		return(istat);

	}





	else
	{
		puterr2("ERROR: unrecognized observation file type", ftype_obs);
		return(OBS_FILE_END_OF_INPUT);
	}
}


/** function remove whitespace from string */

void removeSpace(char *str) {

	int n, m;

	n = 0;
	while (str[n] != '\0' && n < 1000000)
		if (isspace(str[n])) {
		m = n;
		while (str[m] != '\0' && m < 1000000) {
			str[m] = str[m + 1];
			m++;
		}
		} else {
			n++;
		}

}


/** function to check if phase_in matches phase_check
true if phase_in == phase_check
true if phase_in is in phase ID list for phase_check */

int IsPhaseID(char *phase_in, char *phase_check)
{
	int npha_id;
	char test_str[PHASE_LABEL_LEN + 4];

//printf("phase_in %s, phase_check %s, ", phase_in, phase_check);
	/* check for blank phase_in */
	if (strstr("              ", phase_in) != NULL)
//{printf(" NO - BLANK\n");
		return(0);
//}

	// check if phase_in == phase_check (AJL 20041201 added)
	if (strcmp(phase_in, phase_check) == 0)
		return(1);

	// check if phase_in is in phase ID list for phase_check
	removeSpace(phase_in);
	sprintf(test_str, " %s ", phase_in);
	for (npha_id = 0; npha_id < NumPhaseID; npha_id++) {
//printf(" compare (<%s> <%s>)\n", PhaseID[npha_id].id_string, test_str);
		if (strcmp(PhaseID[npha_id].phase, phase_check) == 0) {
			if (strstr(PhaseID[npha_id].id_string, test_str) != NULL)
//{printf(" YES (%s %s)\n", PhaseID[npha_id].id_string,  phase_in);
				return(1);
//}
		}
	}

//printf(" NO\n");
	return(0);
}



/** function to evaluate phase ID from phase ID lists */

int EvalPhaseID(char *phase_out, char *phase_in)
{
	int npha_id;

	for (npha_id = 0; npha_id < NumPhaseID; npha_id++) {
		if (IsPhaseID(phase_in, PhaseID[npha_id].phase)) {
			if (phase_out != NULL)
				strcpy(phase_out, PhaseID[npha_id].phase);
			return(1);
		}
	}

	if (phase_out != NULL)
		strcpy(phase_out, phase_in);

	return(0);	// returned unchanged
}



/** function to check if a date is reasonable */

int IsGoodDate(int iyear, int imonth, int iday)
{
	if (iyear >= SMALLEST_EVENT_YEAR && iyear <= LARGEST_EVENT_YEAR
		   && imonth > 0 && imonth < 13
		   && iday > 0 && iday < 32)
		return(1);

	return(0);
}



/** function to homogenize date / time of arrivals */

int HomogDateTime(ArrivalDesc *arrival, int num_arrivals, HypoDesc* phypo)
{
	int narr;
	int dofymin = 10000, yearmin = 10000;
	int test_month, test_day;

	for (narr = 0; narr < num_arrivals; narr++) {
		if (arrival[narr].year < yearmin)
			yearmin = arrival[narr].year;
		// AJL 20060615 - now allow crossing of year boundary (requires that first reading is earlier year, etc...)
		if (arrival[narr].year != yearmin) {
			// ok if Dec 31 -> Jan 01
			if ((arrival[narr].year == yearmin + 1)
						  && (arrival[narr].month == 1) && (arrival[narr].day == 1)) {
				arrival[narr].year = yearmin;
				arrival[narr].month = 12;
				arrival[narr].day = 31;
				arrival[narr].hour += 24;
						  } else {
							  return(OBS_FILE_ARRIVALS_CROSS_YEAR_BOUNDARY);
						  }
		}
		arrival[narr].day_of_year =
				DayOfYear(arrival[narr].year, arrival[narr].month, arrival[narr].day);
		if (arrival[narr].day_of_year < dofymin)
			dofymin = arrival[narr].day_of_year;
	}

	for (narr = 0; narr < num_arrivals; narr++) {
		if (arrival[narr].day_of_year > dofymin) {
			arrival[narr].day_of_year--;
			arrival[narr].day--;
			arrival[narr].hour += 24;
		}
	}

	for (narr = 0; narr < num_arrivals; narr++)
		arrival[narr].obs_time = (long double) arrival[narr].sec
//			- (long double) arrival[narr].delay	// DELAY_CORR	- incorporating delay so subtract (Tcorr = Tobs - (O-C))
				+ 60.0L * ((long double) arrival[narr].min
				+ 60.0L * (long double) arrival[narr].hour);

	if (!FixOriginTimeFlag) {
		/* initialize hypocenter year/month/day if origin time not fixed */
		phypo->year = yearmin;
		MonthDay(yearmin, dofymin, &(phypo->month), &(phypo->day));
	} else {
		/* homogenize hypocenter otime if origin time fixed */
		MonthDay(yearmin, dofymin, &test_month, &test_day);
		if (phypo->year != yearmin || test_month != phypo->month
				  || test_day != phypo->day) {
			puterr(
					"ERROR: earliest arrivals year/month/day does not match fixed origin time year/month/day, ignoring observation set.");
			return(OBS_FILE_ARRIVALS_CROSS_YEAR_BOUNDARY);
				  }
				  phypo->time = (long double) phypo->sec
						  + 60.0L * ((long double) phypo->min
						  + 60.0L * (long double) phypo->hour);
				  phypo->min = 0;
				  phypo->hour = 0;
	}

	return(0);


}




/** function to check for arrivals with no absolute timing */

int CheckAbsoluteTiming(ArrivalDesc *arrival, int num_arrivals)
{
	int narr;
	int nNoAbs = 0;

	for (narr = 0; narr < num_arrivals; narr++) {
		if (arrival[narr].inst[0] == '*') {
			arrival[narr].abs_time = 0;
			nNoAbs++;
		} else {
			arrival[narr].abs_time = 1;
		}

	}

	return(nNoAbs);


}




/** function to standardize date / time of arrivals, caclulate rms */

int StdDateTime(ArrivalDesc *arrival, int num_arrivals, HypoDesc* phypo)
{
	int narr;
	double rms_resid = 0.0, weight_sum = 0.0;
	long double sec_tmp, hyp_time_tmp;


	for (narr = 0; narr < num_arrivals; narr++) {
		// calc obs travel time and residual for arrivals with finite travel time and abs timing
		// AJL 20050926 bug fix - do not update rms with arrivals with no travel time!
		// if (arrival[narr].abs_time) {
		if (arrival[narr].abs_time && arrival[narr].pred_travel_time > 0.0) {
			arrival[narr].obs_travel_time =
					arrival[narr].obs_time - phypo->time;
			arrival[narr].residual = arrival[narr].obs_travel_time -
					arrival[narr].pred_travel_time;
			rms_resid += arrival[narr].weight *
					arrival[narr].residual * arrival[narr].residual;
			weight_sum += arrival[narr].weight;
		} else {
			arrival[narr].obs_travel_time = 0.0;
			arrival[narr].residual = 0.0;
		}
		/* convert time to year/month/day/hour/min */
// DELAY_CORR		sec_tmp = arrival[narr].obs_time;
		// removing delay so add (Tobs = Tcorr + (O-C))
		sec_tmp = arrival[narr].obs_time + arrival[narr].delay;
		arrival[narr].hour = (int) (sec_tmp / 3600.0L);
		sec_tmp -= (long double) arrival[narr].hour * 3600.0L;
		arrival[narr].min = (int) (sec_tmp / 60.0L);
		sec_tmp -= (long double) arrival[narr].min * 60.0L;
		arrival[narr].sec = (double) sec_tmp;
		MonthDay(arrival[narr].year, arrival[narr].day_of_year,
			 &(arrival[narr].month), &(arrival[narr].day));
	}

	// set rms if not set earlier in SaveBestLocation
	if (phypo->rms < 0.0) {
		phypo->rms = 999.99;
		if (weight_sum > 0.0)
			phypo->rms = sqrt(rms_resid / weight_sum);
	}

	hyp_time_tmp = phypo->time;
	phypo->hour = (int) (hyp_time_tmp / 3600.0L);
	hyp_time_tmp -= (long double) phypo->hour * 3600.0L;
	phypo->min = (int) (hyp_time_tmp / 60.0L);
	hyp_time_tmp -= (long double) phypo->min * 60.0L;
	phypo->sec = (double) hyp_time_tmp;

	return(0);

}




/** function to set output file root name using arrival time */

int SetOutName(ArrivalDesc *arrival, char* out_file_root, char* out_file,
	       char* lastfile, int isec, int *pncount)
{

	char filename_ctr[10];

/*	if (isec)
	sprintf(out_file, "%s.%4.4d%2.2d%2.2d.%2.2d%2.2d%2.2d",
	out_file_root, arrival->year, arrival->month, arrival->day,
	arrival->hour, arrival->min, (int) arrival->sec);
	else
	sprintf(out_file, "%s.%4.4d%2.2d%2.2d.%2.2d%2.2d",
	out_file_root, arrival->year, arrival->month, arrival->day,
	arrival->hour, arrival->min);  */
	/* SH 03/28/02 change to include digits of sec to construct filename */
	if (isec)
		sprintf(out_file, "%s.%4.4d%2.2d%2.2d.%2.2d%2.2d%05.2f",
			out_file_root, arrival->year, arrival->month, arrival->day,
			arrival->hour, arrival->min, arrival->sec);
	else
		sprintf(out_file, "%s.%4.4d%2.2d%2.2d.%2.2d%2.2d%2.2d",
			out_file_root, arrival->year, arrival->month, arrival->day,
			arrival->hour, arrival->min, (int) arrival->sec);
/* SH 04/08/02 check if same filename as previous event;
	if so append 'b' to filename;
	identical filenames can happen with swarm data */
	//printf(">>>>>>%s<>%s<\n", out_file, lastfile);
	//if (ncount++ > 0 || strcmp(out_file, lastfile) == 0) {
	// AJL 20060615 bug fix!  Following line added
	if (strcmp(out_file, lastfile) == 0) {
		strcpy(lastfile, out_file); /* save filename */
		sprintf(filename_ctr, "_%3.3d", *pncount);
		strcat(out_file, filename_ctr);
		(*pncount)++;
	} else {
		strcpy(lastfile, out_file); /* save filename */
		*pncount = 1;
	}
	return(0);

}


/** function to check for duplicate label and phase (and time) in arrival */

int IsDuplicateArrival(ArrivalDesc *arrival, int num_arrivals, int ntest, int rejectOnlyForExactTimeMatch)
{
	int narr;

	for (narr = 0; narr < num_arrivals; narr++) {
		if (narr != ntest
				  && !strcmp(arrival[narr].time_grid_label, arrival[ntest].time_grid_label)
				  && !strcmp(arrival[narr].phase, arrival[ntest].phase)) {
			if (rejectOnlyForExactTimeMatch) {
				if (fabs(arrival[narr].sec - arrival[ntest].sec) <=
								((arrival[narr].error + arrival[ntest].error) / 2.0)) {
					if (arrival[narr].min == arrival[ntest].min &&
									       arrival[narr].hour == arrival[ntest].hour &&
									       arrival[narr].day == arrival[ntest].day &&
									       arrival[narr].month == arrival[ntest].month &&
									       arrival[narr].year == arrival[ntest].year
					   )
						return(narr);
								}
			} else {
				return(narr);
			}
				  }
	}

	return(-1);

}



/** function to check for duplicate label and phase in arrival */

int IsSameArrival(ArrivalDesc *arrival, int num_arrivals, int ntest, char *phase_test)
{
	int narr;

	if (phase_test == NULL) {
		for (narr = 0; narr < num_arrivals; narr++) {
			if (narr != ntest
						 && ((IsPhaseID(arrival[narr].phase, "P") &&
						 IsPhaseID(arrival[ntest].phase, "P"))
						 || (IsPhaseID(arrival[narr].phase, "S") &&
						 IsPhaseID(arrival[ntest].phase, "S")))
						 && !strcmp(arrival[narr].time_grid_label, arrival[ntest].time_grid_label))
				return(narr);
		}
	} else {
		for (narr = 0; narr < num_arrivals; narr++) {
			if (narr != ntest
						 && !strcmp(arrival[narr].time_grid_label, arrival[ntest].time_grid_label)
						 && IsPhaseID(arrival[narr].phase, phase_test))
				return(narr);
		}
	}

	return(-1);

}



/** function to check for duplicate label and phase in arrival */

int FindDuplicateTimeGrid(ArrivalDesc *arrival, int num_arrivals, int ntest)
{
	int narr;

	for (narr = 0; narr < num_arrivals; narr++) {
		if (narr != ntest
				  && !strcmp(arrival[narr].fileroot, arrival[ntest].fileroot)
				  && arrival[narr].flag_ignore == 0
		   )
			return(narr);
	}

	return(-1);

}



/** function to perform grid search location */

int LocGridSearch(int ngrid, int num_arr_total, int num_arr_loc,
		  ArrivalDesc *arrival,
		  GridDesc* ptgrid, GaussLocParams* gauss_par, HypoDesc* phypo)
{

	int istat;
	int ix = -1, iy = -1, iz = -1, narr;
	int iGridType;
	int nReject, numGridReject = 0, numStaReject = 0;
	double xval, yval, zval;
	/*double travel_time;*/
	double value;
	double misfit;
	double misfit_min = VERY_LARGE_DOUBLE, misfit_max = -VERY_LARGE_DOUBLE;
	double dlike;

	double value_search = -1.0;	// not used here




	/* get solution quality at each grid point */

	if (message_flag >= 4) {
		putmsg(4, "");
		putmsg(4, "Calculating solution over grid...");
	}

	iGridType = ptgrid->type;

	xval = ptgrid->origx;

	/* loop over grid points */

	for (ix = 0; ix <  ptgrid->numx; ix++) {

		/* read y-z sheets for arrival travel-times (3D grids) */
		if ((istat = ReadArrivalSheets(num_arr_loc, arrival, xval)) < 0)
			puterr("ERROR: reading arrival travel time sheets.");

		yval = ptgrid->origy;
		for (iy = 0; iy <  ptgrid->numy; iy++) {
			zval = ptgrid->origz;
			for (iz = 0; iz <  ptgrid->numz; iz++) {


				// get travel times for observed arrivals

				if (isAboveTopo(xval, yval, zval)) {

					misfit = -1.0;
					value = 0.0;
					if (iGridType == GRID_MISFIT)
						value = -1.0;
					else if (iGridType == GRID_PROB_DENSITY)
						value = -LARGE_FLOAT;
					ptgrid->array[ix][iy][iz] = value;

				} else {

					nReject = getTravelTimes(arrival, num_arr_loc, xval, yval, zval);

					if (nReject) {

						numGridReject++;
						numStaReject += nReject;
						misfit = -1.0;
						value = 0.0;
						if (iGridType == GRID_MISFIT)
							value = -1.0;
						else if (iGridType == GRID_PROB_DENSITY)
							value = -LARGE_FLOAT;
						ptgrid->array[ix][iy][iz] = value;

					} else {

						/* calc misfit or prob density */

						value = CalcSolutionQuality(num_arr_loc,
								arrival, gauss_par,
								iGridType, &misfit, NULL, NULL, 0.0, &value_search);
						ptgrid->array[ix][iy][iz] = value;
						if (iGridType == GRID_MISFIT) {
							ptgrid->sum += value;
						} else if (iGridType == GRID_PROB_DENSITY) {
							dlike = exp(value);
							ptgrid->sum += dlike;
							/* update  probabilitic residuals */
							UpdateProbabilisticResiduals(num_arr_loc, arrival, dlike);
						}

						/* check for minimum misfit */
						if (misfit < misfit_min) {
							misfit_min = misfit;
							phypo->misfit = misfit;
							phypo->ix = ix;
							phypo->iy = iy;
							phypo->iz = iz;
							phypo->x = xval;
							phypo->y = yval;
							phypo->z = zval;
							for (narr = 0; narr < num_arr_loc; narr++)
								arrival[narr].pred_travel_time_best =
										arrival[narr].pred_travel_time;
						}
						if (misfit > misfit_max)
							misfit_max = misfit;

					}
				}

				zval += ptgrid->dz;
			}
			yval += ptgrid->dy;
		}
		xval += ptgrid->dx;
	}


	/* give warning if grid points rejected */

	if (numGridReject > 0) {
		sprintf(MsgStr, "WARNING: %d grid locations rejected; travel times for an average of %.2lf arrival observations were not valid.",
			numGridReject, (double) numStaReject / numGridReject);
		putmsg(1, MsgStr);
	}


	/* construct search information string */
	sprintf(phypo->searchInfo, "GRID nPts %d%c", ix * iy *iz, '\0');
	/* write message */
	/*putmsg(2, phypo->searchInfo);*/


	/* re-calculate solution and arrival statistics for best location */

	SaveBestLocation(num_arr_total, num_arr_loc, arrival,  ptgrid,  gauss_par, phypo, misfit_max, iGridType);


	return(0);

}



/** function to perform Metropolis location */

#define MAX_NUM_MET_TRIES 1000

int LocMetropolis(int ngrid, int num_arr_total, int num_arr_loc,
		  ArrivalDesc *arrival,
		  GridDesc* ptgrid, GaussLocParams* gauss_par, HypoDesc* phypo,
		  WalkParams* pMetrop, float* fdata)
{

	int istat;
	int ntry, nSamples, nSampStat, narr, ipos;
	long int ngenerated;
	int maxNumTries;
	int writeMessage = 0;
	int iGridType;
	int nReject, numClipped = 0, numGridReject = 0, numStaReject = 0;
	int iAbort = 0, iReject = 0;
	int iBoundary = 0;
	int iAccept, numAcceptDeepMinima = 0;
	double xval, yval, zval;
	double currentMetStepFact;

	double value, dlike, dlike_max = -VERY_LARGE_DOUBLE;

	double misfit;
	double misfit_min = VERY_LARGE_DOUBLE, misfit_max = -VERY_LARGE_DOUBLE;

	double xmin, xmax, ymin, ymax, zmin, zmax;
	double dx_init, dx_test;

	int nScatterSaved;

	double xmean_sum = 0.0, ymean_sum = 0.0, zmean_sum = 0.0;
	double xvar_sum = 0.0, yvar_sum = 0.0, zvar_sum = 0.0;
	double xvar = 0.0, yvar = 0.0, zvar = 0.0;
	double dsamp = 0.0, dsamp2;

	double value_search = -1.0;	// not used here



	/* get solution quality at each sample on random walk */

	if (message_flag >= 4) {
		putmsg(4, "");
		putmsg(4, "Calculating solution along Metropolis walk...");
	}

	iGridType = GRID_PROB_DENSITY;

	/* set walk limits equal to grid limits */
	xmin = ptgrid->origx;
	xmax = xmin + (double) (ptgrid->numx - 1) * ptgrid->dx;
	ymin = ptgrid->origy;
	ymax = ymin + (double) (ptgrid->numy - 1) * ptgrid->dy;
	zmin = ptgrid->origz;
	zmax = zmin + (double) (ptgrid->numz - 1) * ptgrid->dz;

	/* save intiial values */
	currentMetStepFact = MetStepFact;
	dx_init = pMetrop->dx;


	/* loop over walk samples */

	nSamples = 0;
	nSampStat = 0;
	nScatterSaved = 0;
	ipos = 0;
	ntry = 0;
	ngenerated = 0;
	maxNumTries = MAX_NUM_MET_TRIES;
	while (nSamples < MetNumSamples
		      && (nSamples <= MetLearn || ntry < maxNumTries)) {

		ntry++;
		ngenerated++;
		istat = GetNextMetropolisSample(pMetrop,
				xmin, xmax, ymin, ymax,
				zmin, zmax, &xval, &yval, &zval);
		if (nSamples > MetEquil && istat > 0)
			numClipped += istat;

		/* get travel times for observed arrivals */

		if (isAboveTopo(xval, yval, zval)) {

			misfit = -1.0;
			dlike = 0.0;

		} else {

			nReject = getTravelTimes(arrival, num_arr_loc, xval, yval, zval);

			if (nReject) {
				numGridReject++;
				numStaReject += nReject;
				misfit = -1.0;
				dlike = 0.0;
			} else {

				/* calc misfit or prob density */
				value = CalcSolutionQuality(num_arr_loc, arrival, gauss_par,
						iGridType, &misfit, NULL, NULL, 0.0, &value_search);
				dlike = gauss_par->WtMtrxSum * exp(value);

				/* apply Metropolis test */
				iAccept = MetropolisTest(pMetrop->likelihood, dlike);

				/* if not accepted, but at maxNumTries... */
				if (!iAccept && ntry == maxNumTries) {
					/* if not learning, accept anyway since
					may be stuck in a deep minima */
					if (nSamples >= MetLearn && numAcceptDeepMinima++ < 5) {
						iAccept = 1;
	//printf("Max Num Tries: accept deep minima\n");

						/* try reducing step size */
						currentMetStepFact /= 2.0;
						ntry = 0;
	//printf("            +: step ch: was %lf\n", pMetrop->dx);

						/*if (pMetrop->dx > MetStepMin) {
						pMetrop->dx /= 2.0;
	//printf("            +: step ch: %lf -> %lf\n", 2.0 * pMetrop->dx, pMetrop->dx);
						ntry = 0;
									}*/

						/* if learning, try reducing step size */
					} else if (nSamples < MetLearn && pMetrop->dx > MetStepMin) {
						pMetrop->dx /= 2.0;
	//printf("Max Num Tries: step ch: %lf -> %lf\n", 2.0 * pMetrop->dx, pMetrop->dx);
						ntry = 0;
					}
				}


				if (iAccept) {

					ntry = 0;
					nSamples++;

					/* check for minimum misfit */
					if (misfit < misfit_min) {
						misfit_min = misfit;
						dlike_max = dlike;
						phypo->misfit = misfit;
						phypo->x = xval;
						phypo->y = yval;
						phypo->z = zval;
						for (narr = 0; narr < num_arr_loc; narr++)
							arrival[narr].pred_travel_time_best =
									arrival[narr].pred_travel_time;
					}
					if (misfit > misfit_max)
						misfit_max = misfit;

					/* update sample location */
					pMetrop->x = xval;
					pMetrop->y = yval;
					pMetrop->z = zval;
					pMetrop->likelihood = dlike;

					/* if learning, update sample statistics */
					if (nSamples > MetLearn / 2 && nSamples <= MetLearn + MetEquil) {

						xmean_sum += xval;
						ymean_sum += yval;
						zmean_sum += zval;
						xvar_sum += xval * xval;
						yvar_sum += yval * yval;
						zvar_sum += zval * zval;
						nSampStat++;
					}

					/* if equilibrating, update Met step */
					if (nSamples > MetLearn
							&& nSamples <= MetLearn + MetEquil) {

						/* update Met step */
						dsamp = (double) nSampStat;
						dsamp2 = dsamp * dsamp;
						xvar = xvar_sum  / dsamp -
							xmean_sum * xmean_sum / dsamp2;
						yvar = yvar_sum  / dsamp -
							ymean_sum * ymean_sum / dsamp2;
						zvar = zvar_sum  / dsamp -
							zmean_sum * zmean_sum / dsamp2;
						dx_test = currentMetStepFact * pow(
							sqrt(xvar) * sqrt(yvar) * sqrt(zvar)
							/ (double) MetUse, 1.0/3.0);
							/*/ (double) (MetUse / MetSkip),*/
//if (pMetrop->dx != dx_test) printf("equil step ch: %lf -> %lf\n", pMetrop->dx, dx_test);

						if (dx_test > MetStepMin)
							pMetrop->dx = dx_test;
						else
							pMetrop->dx = MetStepMin;
							}

							/* if saving samples */
							if (nSamples > MetStartSave
							&& nSamples % MetSkip == 0) {

							/* save sample to scatter file */
							fdata[ipos++] = xval;
							fdata[ipos++] = yval;
							fdata[ipos++] = zval;
							fdata[ipos++] = dlike;

							/* update  probabilitic residuals */
							if (1)
							UpdateProbabilisticResiduals(
							num_arr_loc, arrival, 1.0);


							nScatterSaved++;
							}

							if (nSamples % 1000 == 1
							|| nSamples == MetLearn / 2)
							writeMessage = 1;

				}


				if (writeMessage || ntry == maxNumTries - 1) {
					if (message_flag >= 4) {
						sprintf(MsgStr,
								"Metropolis: n %d x %.2lf y %.2lf z %.2lf  xm %.2lf ym %.2lf zm %.2lf  xdv %.2lf ydv %.2lf zdv %.2lf  dx %.2lf  li %.2le", nSamples, pMetrop->x, pMetrop->y, pMetrop->z, xmean_sum / dsamp, ymean_sum / dsamp, zmean_sum / dsamp, sqrt(xvar), sqrt(yvar), sqrt(zvar), pMetrop->dx, pMetrop->likelihood);
						putmsg(4, MsgStr);
					}
					writeMessage = 0;
				}

			}
		}


		/* check abort search conditions */

		/* failure to accept sample after maxNumTries */
		if (nSamples > MetLearn && ntry >= maxNumTries) {
			sprintf(MsgStr,
				"ERROR: failed to accept new Metropolis sample after %d tries, aborting location.", ntry);
			puterr(MsgStr);
			sprintf(phypo->locStatComm, "%s", MsgStr);
			iAbort = 1;
			break;
		}

		/* maximum likelihood too low after learning stage */
		if (nSamples == MetLearn && dlike_max < MetProbMin) {
			sprintf(MsgStr,
				"ERROR: after learning stage (%d samples), best probability = %.2le is less than ProbMin = %.2le, aborting location.",
				MetLearn, dlike_max, MetProbMin);
			puterr(MsgStr);
			sprintf(phypo->locStatComm, "%s", MsgStr);
			iAbort = 1;
			break;
		}

		      }


		      /* give warning if sample points clipped */

		      if (numClipped > 0) {
			      sprintf(MsgStr, "WARNING: %d Metropolis samples clipped at search grid boundary.",
				      numClipped);
			      putmsg(1, MsgStr);
		      }


		      /* give warning if grid points rejected */

		      if (numGridReject > 0) {
			      sprintf(MsgStr, "WARNING: %d Metropolis samples rejected; travel times for an average of %.2lf arrival observations were not valid.",
				      numGridReject, (double) numStaReject / numGridReject);
			      putmsg(1, MsgStr);
		      }


		      /* check reject location conditions */

		      /* maximum like hypo on edge of grid */
		      if ((iBoundary = isOnGridBoundary(phypo->x, phypo->y, phypo->z,
			   ptgrid, pMetrop->dx, pMetrop->dx, 0))) {
				   sprintf(MsgStr, "WARNING: max prob location on grid boundary %d, rejecting location.", iBoundary);
				   putmsg(1, MsgStr);
				   sprintf(phypo->locStatComm, "%s", MsgStr);
				   iReject = 1;
			   }

			   /* construct search information string */
			   sprintf(phypo->searchInfo,
				   "METROPOLIS nSamp %ld nAcc %d nSave %d nClip %d Dstep0 %lf Dstep %lf%c",
				   ngenerated, nSamples, nScatterSaved, numClipped, dx_init, pMetrop->dx, '\0');
			   /* write message */
			   putmsg(2, phypo->searchInfo);


			   /* check for termination */
			   if (iAbort) {
				   sprintf(Hypocenter.locStat, "ABORTED");
			   } else if (iReject) {
				   sprintf(Hypocenter.locStat, "REJECTED");
			   }


			   /* re-calculate solution and arrival statistics for best location */

			   SaveBestLocation(num_arr_total, num_arr_loc, arrival,  ptgrid,
					    gauss_par, phypo, misfit_max, iGridType);


			   return(nScatterSaved);

}




/** function to create next metropolis sample */
/* move sample random distance and direction */

INLINE int GetNextMetropolisSample(WalkParams* pMetrop, double xmin, double xmax,
				   double ymin, double ymax, double zmin, double zmax,
				   double* pxval, double* pyval, double* pzval)
{

	int iClip = 0;
	double valx, valy, valz, valsum, norm;
	double x, y, z;


	/* get unit vector in random direction */

	do {
		valx = get_rand_double(-1.0, 1.0);
		valy = get_rand_double(-1.0, 1.0);
		valz = get_rand_double(-1.0, 1.0);
		valsum = valx * valx + valy * valy + valz * valz;
	} while (valsum < SMALL_DOUBLE);

	norm = pMetrop->dx / sqrt(valsum);

	/* add step to last sample location */

	x = pMetrop->x + norm * valx;
	y = pMetrop->y + norm * valy;
	z = pMetrop->z + norm * valz;


	/* crude clip against grid boundary */
	/* clip needed because travel time lookup requires that
	location is within initial search grid */
	if (x < xmin) {
		x = xmin;
		iClip = 1;
	} else if (x > xmax) {
		x = xmax;
		iClip = 1;
	}
	if (y < ymin) {
		y = ymin;
		iClip = 1;
	} else if (y > ymax) {
		y = ymax;
		iClip = 1;
	}
	if (z < zmin) {
		z = zmin;
		iClip = 1;
	} else if (z > zmax) {
		z = zmax;
		iClip = 1;
	}


	/* update sample location */

	*pxval = x;
	*pyval = y;
	*pzval = z;

	return(iClip);

}


/** function to test new metropolis string */

INLINE int MetropolisTest(double likelihood_last, double likelihood_new)
{

	double prob;

	/* compare with last sample using Mosegaard & Tarantola eq (17) */

	if (likelihood_new >= likelihood_last)
		return(1);
	else if ((prob = get_rand_double(0.0, 1.0)) < likelihood_new / likelihood_last)
		return(1);
	else
		return(0);

}


/** function to re-calculate solution and arrival statistics for best location */
/* some quantities are calculated only for arrivals used in location
		(num_arr_loc) others for all arrivals (num_arr_total) */

int SaveBestLocation(int num_arr_total, int num_arr_loc, ArrivalDesc *arrival,
		     GridDesc* ptgrid, GaussLocParams* gauss_par, HypoDesc* phypo,
		     double misfit_max, int iGridType)
{
	int istat, narr, n_compan, iopened;
	char filename[FILENAME_MAX];
	double value, misfit, otime, otime_var;

	double value_search = -1.0;	// not used here

	SourceDesc station;



//printf("SaveBestLocation num_arr_total %d num_arr_loc %d\n", num_arr_total, num_arr_loc);

	/* loop over observed arrivals */
	for (narr = 0; narr < num_arr_total; narr++) {
		arrival[narr].dist = GetEpiDist(&(arrival[narr].station), phypo->x, phypo->y);
		// 20060619 AJL - dist changed to always km, output converted to degrees for GLOBAL in
		//if (GeometryMode == MODE_GLOBAL)
		//	arrival[narr].dist *= KM2DEG;
		arrival[narr].azim = GetEpiAzim(&(arrival[narr].station), phypo->x, phypo->y);

		/* get best travel time */

		arrival[narr].pred_travel_time = 0.0;
		n_compan = arrival[narr].n_companion;
		iopened = 0;
		/* check for stored best travel time */
		if (arrival[narr].pred_travel_time_best > 0.0) {
			/* load stored best travel time */
			arrival[narr].pred_travel_time = arrival[narr].pred_travel_time_best;
			/* check for companion travel time */
		} else if (n_compan >= 0 && arrival[n_compan].pred_travel_time_best > 0.0) {
			/* load companion stored best travel time */
			arrival[narr].pred_travel_time =
					arrival[n_compan].pred_travel_time_best;
			arrival[narr].pred_travel_time *= arrival[narr].tfact;
		} else {
			/* temporarily open time grid file
			and read time for ignored arrivals */
			// save staion information (will be overwritten in OpenGrid3dFile()
			station = arrival[narr].station;
			sprintf(filename, "%s.time", arrival[narr].fileroot);
			if ((istat = OpenGrid3dFile(filename,
			     &(arrival[narr].fpgrid),
			     &(arrival[narr].fphdr),
			     &(arrival[narr].gdesc), "time",
			     &(arrival[narr].station),
			     iSwapBytesOnInput)) < 0)
				continue;
			arrival[narr].station = station;
			//iopened = 1;
			/* check grid type, read travel time */
			if (arrival[narr].gdesc.type == GRID_TIME) {
				/* 3D grid */
				if (arrival[narr].fpgrid != NULL)
					arrival[narr].pred_travel_time = (double) ReadAbsInterpGrid3d(
							arrival[narr].fpgrid, &(arrival[narr].gdesc),
				phypo->x, phypo->y, phypo->z);
				if (arrival[narr].pred_travel_time < -LARGE_DOUBLE)
					arrival[narr].pred_travel_time = 0.0;
			} else {
				/* 2D grid (1D model) */
				// AJL 20060602 dist stored as km, KM2DEG added
				if (arrival[narr].fpgrid != NULL)
					arrival[narr].pred_travel_time =
							ReadAbsInterpGrid2d(
							arrival[narr].fpgrid, &(arrival[narr].gdesc),
				GeometryMode == MODE_GLOBAL ? arrival[narr].dist * KM2DEG : arrival[narr].dist,
				phypo->z);
				if (arrival[narr].pred_travel_time < -LARGE_DOUBLE)
					arrival[narr].pred_travel_time = 0.0;
			}
			arrival[narr].pred_travel_time *= arrival[narr].tfact;
			// apply crustal correction
			if (ApplyCrustElevCorrFlag && GeometryMode == MODE_GLOBAL
					&& arrival[narr].pred_travel_time > 0.0) {
				if (arrival[narr].dist > MinDistCrustElevCorr)
					arrival[narr].pred_travel_time +=
						applyCrustElevCorrection(arrival + narr, phypo->x, phypo->y, phypo->z);
			}
			else if (ApplyElevCorrFlag) {
				if (arrival[narr].pred_travel_time > 0.0) // ignore arrivals with no pred tt
					arrival[narr].pred_travel_time += arrival[narr].elev_corr;
			}
			CloseGrid3dFile(&(arrival[narr].fpgrid), &(arrival[narr].fphdr));

		}

		/* read angles */
		/* angle grid file name */
		if (n_compan >= 0)
			sprintf(filename, "%s.angle", arrival[n_compan].fileroot);
		else
			sprintf(filename, "%s.angle", arrival[narr].fileroot);
		if (angleMode == ANGLE_MODE_YES) {
			if (arrival[narr].gdesc.type == GRID_TIME) {
				/* 3D grid */
				ReadTakeOffAnglesFile(filename,
						phypo->x, phypo->y, phypo->z,
						&(arrival[narr].ray_azim),
						&(arrival[narr].ray_dip),
						&(arrival[narr].ray_qual), -1.0, iSwapBytesOnInput);
			} else {
				/* 2D grid (1D model) */
				// AJL 20060828 dist stored as km, KM2DEG added
				ReadTakeOffAnglesFile(filename,
						0.0,
						GeometryMode == MODE_GLOBAL ? arrival[narr].dist * KM2DEG : arrival[narr].dist,
						phypo->z,
						&(arrival[narr].ray_azim),
						&(arrival[narr].ray_dip),
						&(arrival[narr].ray_qual), arrival[narr].azim, iSwapBytesOnInput);
			}
		}

		//		/* close time grid file for ignored arrivals */
//		if (iopened)
//			CloseGrid3dFile(&(arrival[narr].fpgrid),
//				&(arrival[narr].fphdr));
	}

	/* calc misfit or prob density */
	otime_var = -1.0;
	value = CalcSolutionQuality(num_arr_loc, arrival, gauss_par, iGridType, &misfit, &otime, &otime_var, 0.0, &value_search);

	// set rms if otime variance is available
	if (otime_var > 0.0)
		phypo->rms = sqrt(otime_var);
	else
		phypo->rms = -1.0;

	/* set origin time */
	if (!FixOriginTimeFlag)
		phypo->time = otime;
	if (iGridType == GRID_PROB_DENSITY) {
		phypo->probmax = exp(value);
	}

	/* set misc hypo fields */
	phypo->grid_misfit_max = misfit_max;
	istat = rect2latlon(0, phypo->x, phypo->y, &(phypo->dlat), &(phypo->dlong));
	phypo->depth = phypo->z;
	phypo->nreadings = num_arr_loc;


	return(0);

}


/** function to read y-z travel time sheet from disk for each arrival */

int ReadArrivalSheets(int num_arrivals, ArrivalDesc *arrival, double xsheet)
{

	int istat, narr, ixsheet;
	float **array_tmp;
	double sheet_origx, sheet_dx;


	/* loop over arrivals */

	for (narr = 0; narr < num_arrivals; narr++) {

		/* skip sheet read if arrival has companion */
		if (arrival[narr].n_companion >= 0)
			continue;

		/* skip sheet read or set xsheet to zero for 2D grid */
		if (arrival[narr].gdesc.type == GRID_TIME_2D) {
			if (arrival[narr].sheetdesc.origx < LARGE_DOUBLE)
				continue;
			xsheet = 0.0;
		}

		sheet_origx = arrival[narr].sheetdesc.origx;
		sheet_dx = arrival[narr].sheetdesc.dx;


		/* check which sheets are required from disc */

		/* both required sheets already read */
		if (sheet_origx <= xsheet && xsheet < sheet_origx + sheet_dx)
			continue;

		/* find x index in disk grid of lower plane of dual sheet */
		if (arrival[narr].gdesc.numx > 1)
			ixsheet = (int) ((xsheet - arrival[narr].gdesc.origx)
						/ arrival[narr].gdesc.dx);
		else
			ixsheet = 0;
		if (ixsheet < 0 || ixsheet > arrival[narr].gdesc.numx - 1) {
			puterr("WARNING: invalid ixsheet value:");
			sprintf(MsgStr, "  Arr: %d  ixsheet: %d", narr, ixsheet);
			puterr(MsgStr);
		}

		/* one required sheet already read */
		if (sheet_origx + sheet_dx <= xsheet &&
				  xsheet < sheet_origx + 2.0 * sheet_dx)
		{
			/* exchange sheet pointers */
			array_tmp =  arrival[narr].sheetdesc.array[0];
			arrival[narr].sheetdesc.array[0] =
					arrival[narr].sheetdesc.array[1];
			arrival[narr].sheetdesc.array[1] = array_tmp;

			/* read next sheet if xsheet not exactly on last sheet */
			/*			if (fabs(xsheet - (sheet_origx + sheet_dx)) */
			/*					> VERY_SMALL_DOUBLE) { */
			/* read new sheet */
			if ((istat =
						  ReadGrid3dBufSheet(
						  arrival[narr].sheetdesc.array[1][0],
			&(arrival[narr].gdesc),
			arrival[narr].fpgrid, ixsheet + 1)) < 0)
				puterr(
						"ERROR: reading new arrival travel time sheet.");
			/*			} */

			/* set dual-sheet origin */
			arrival[narr].sheetdesc.origx += sheet_dx;
		}

		/* no required sheets already read */
		else
		{

			/* read lower sheet */
			if ((istat =
						  ReadGrid3dBufSheet(
						  arrival[narr].sheetdesc.array[0][0],
			&(arrival[narr].gdesc),
			arrival[narr].fpgrid, ixsheet)) < 0)
				puterr(
						"ERROR: reading lower arrival travel time sheet.");

			/* read upper sheet if not at last sheet */
			if (ixsheet + 1 < arrival[narr].gdesc.numx) {
				if ((istat =
								 ReadGrid3dBufSheet(
								 arrival[narr].sheetdesc.array[1][0],
				&(arrival[narr].gdesc),
				arrival[narr].fpgrid, ixsheet + 1)) < 0)
					puterr(
							"ERROR: reading upper arrival travel time sheet.");
			}

			/* set dual-sheet origin */
			arrival[narr].sheetdesc.origx =
					(double) ixsheet * sheet_dx
					+ arrival[narr].gdesc.origx;
		}

		/*Narr %d O %lf %lf %lf  N %d %d %d  dx %lf %lf %lf\n", narr, arrival[narr].sheetdesc.origx, arrival[narr].sheetdesc.origy, arrival[narr].sheetdesc.origz, arrival[narr].sheetdesc.numx, arrival[narr].sheetdesc.numy, arrival[narr].sheetdesc.numz, arrival[narr].sheetdesc.dx, arrival[narr].sheetdesc.dy, arrival[narr].sheetdesc.dz);*/
	}

	return(0);

}




/** function to consruct weight matrix (inverse of covariance matrix) */

int ConstWeightMatrix(int num_arrivals, ArrivalDesc *arrival,
		      GaussLocParams* gauss_par)
{
	int istat, nrow, ncol;
	double sigmaT2, corr_len2;
	int corr_len_nonzero = 1;
	double dx, dy, dz, dist2;
	double weight_sum;
	double sta_wt;
	DMatrix null_mtrx = NULL;
	SourceDesc *sta1, *sta2;
	double arrivalWeightMax = -1.0;

	static DMatrix wt_matrix, edt_matrix = NULL;
	static int last_matrix_alloc_size = -1;


	// free old matrices
	if (last_matrix_alloc_size > 0) {
		free_dmatrix(edt_matrix, 0, last_matrix_alloc_size, 0, last_matrix_alloc_size);
		free_dmatrix(wt_matrix, 0, last_matrix_alloc_size, 0, last_matrix_alloc_size);
	}
	last_matrix_alloc_size = num_arrivals - 1;
	// allocate square matrices
	edt_matrix = dmatrix(0, num_arrivals - 1, 0, num_arrivals - 1);
	wt_matrix = dmatrix(0, num_arrivals - 1, 0, num_arrivals - 1);


	/* set constants */

	sigmaT2 = gauss_par->SigmaT * gauss_par->SigmaT;
	corr_len2 = gauss_par->CorrLen * gauss_par->CorrLen;
	// AJL 20041201 - corr_len_nonzero flag added, before corr_len2 was set to 1.0 (was bug?)
	if (corr_len2 < VERY_SMALL_DOUBLE || gauss_par->CorrLen < 0.0) {
		corr_len_nonzero = 0;
		sprintf(MsgStr, "LOCGAU param CorrLen is zero, will not be used: %lf", gauss_par->CorrLen);
		putmsg(1, MsgStr);
	} else {
		corr_len_nonzero = 1;
		sprintf(MsgStr, "LOCGAU param CorrLen is non-zero, will be used: %lf", gauss_par->CorrLen);
		putmsg(1, MsgStr);
	}


	/* load covariances */

	for (nrow = 0; nrow < num_arrivals; nrow++) {
		sta1 = &(arrival[nrow].station);
		arrival[nrow].tt_error = gauss_par->SigmaT;
		for (ncol = 0; ncol <= nrow; ncol++) {
			sta2 = &(arrival[ncol].station);

			/* travel time error (TV82, eq. 10-14; MEN92, eq. 22) */
			if (strcmp(arrival[nrow].phase, arrival[ncol].phase) == 0) {
			// same phase types, include spatial correlation
				dx = sta1->x - sta2->x;
				dy = sta1->y - sta2->y;
				dz = sta1->z - sta2->z;
				dist2 = dx * dx + dy * dy + dz * dz;
				if (GeometryMode == MODE_GLOBAL)
					dist2 *= DEG2KM * DEG2KM;
				// EDT
				if (ncol == nrow) {	// diagonal of EDT gets gaussian model time error
					edt_matrix[nrow][ncol] = sigmaT2;
				} else {		// off-diagonal of EDT gets gaussian model weight
					if (corr_len_nonzero)
						edt_matrix[nrow][ncol] = edt_matrix[ncol][nrow] = exp(-0.5 * dist2 / corr_len2);
					else
						edt_matrix[nrow][ncol] = edt_matrix[ncol][nrow] = 0.0;
				}
				// LS/L2 gaussian model time error
			// AJL 20050914 - bug?  added ncol == nrow case so error is non-zero
				if (ncol == nrow) {	// diagonal gets gaussian model time error
					wt_matrix[nrow][ncol] = sigmaT2;
				} else {		// off-diagonal
					if (corr_len_nonzero)
						wt_matrix[nrow][ncol] = wt_matrix[ncol][nrow] = sigmaT2 * exp(-0.5 * dist2 / corr_len2);
					else
						wt_matrix[nrow][ncol] = wt_matrix[ncol][nrow] = 0.0;
				}
			} else {
			// different phase types, assumed no spatial correlation
				edt_matrix[nrow][ncol] = edt_matrix[ncol][nrow] = 0.0;
				wt_matrix[nrow][ncol] = wt_matrix[ncol][nrow] = 0.0;
			}

			/* obs time error */
			if (ncol == nrow) {
				edt_matrix[nrow][ncol] += arrival[nrow].error * arrival[nrow].error;
				wt_matrix[nrow][ncol] += arrival[nrow].error * arrival[nrow].error;
			}

		}
	}

	if (message_flag >= 5)
		DisplayDMatrix("Covariance", wt_matrix, num_arrivals, num_arrivals);


	/* invert covariance matrix to obtain weight matrix */

	if ((istat = dgaussj(wt_matrix, num_arrivals, null_mtrx, 0)) < 0) {
		puterr("ERROR: inverting covariance matrix.");
		return(-1);
	}

	if (message_flag >= 5)
		DisplayDMatrix("Weight", wt_matrix, num_arrivals, num_arrivals);


	// station distance weighting

	for (nrow = 0; nrow < num_arrivals; nrow++) {
//printf("station weight: %s %s %s weight: %lf\n", arrival[nrow].label, arrival[nrow].inst, arrival[nrow].comp, arrival[nrow].station_weight);
		for (ncol = 0; ncol <= nrow; ncol++) {
			sta_wt = (arrival[nrow].station_weight + arrival[ncol].station_weight) / 2.0;
			wt_matrix[nrow][ncol] *= sta_wt;
			wt_matrix[ncol][nrow] *= sta_wt;
		}
	}

	/* get row weights & sum of weights */

	weight_sum = 0.0;
	for (nrow = 0; nrow < num_arrivals; nrow++) {
		arrival[nrow].weight = 0.0;
		for (ncol = 0; ncol < num_arrivals; ncol++) {
			arrival[nrow].weight += wt_matrix[nrow][ncol];
			weight_sum += wt_matrix[nrow][ncol];
//printf("row %d col %d: wt_tx(r,c) %f  arr(row)_wt %f   wt_sum %lf\n", nrow, ncol, wt_matrix[nrow][ncol], arrival[nrow].weight, weight_sum);
		}
	}
	for (nrow = 0; nrow < num_arrivals; nrow++) {
		arrival[nrow].weight = (double) num_arrivals * arrival[nrow].weight / weight_sum;
//printf("observation weight: %s %s %s weight: %lf\n", arrival[nrow].label, arrival[nrow].inst, arrival[nrow].comp, arrival[nrow].weight);
		if (arrival[nrow].weight < 0.0) {
			sprintf(MsgStr,
				"ERROR: negative observation weight: %s %s %s weight: %lf",
				arrival[nrow].label, arrival[nrow].inst,
				arrival[nrow].comp, arrival[nrow].weight);
			puterr(MsgStr);
			puterr("   Gaussian model error (see LOCGAU) may be too large relative to obs uncertainty (see LOCQUAL2ERR, or NLL-Phase format ErrMag).");
		}
		if (arrival[nrow].weight > arrivalWeightMax)
			arrivalWeightMax = arrival[nrow].weight;
	}
	if (message_flag >= 4) {
		sprintf(MsgStr, "Weight Matrix sum: %lf", weight_sum);
		putmsg(4, MsgStr);
	}


	// set global variables
	gauss_par->EDTMtrx = edt_matrix;
	gauss_par->WtMtrx = wt_matrix;
	gauss_par->WtMtrxSum = weight_sum;
	gauss_par->arrivalWeightMax = arrivalWeightMax;

	return(0);

}



/** function to calculate weighted mean of observed arrival times */
/*		(TV82, eq. A-38) */

void CalcCenteredTimesObs(int num_arrivals, ArrivalDesc *arrival,
			  GaussLocParams* gauss_par, HypoDesc* phypo)
{

	int nrow, ncol, narr;
	long double sum, weighted_mean;
	DMatrix wtmtx;
	double *wtmtxrow, wt_sum;


	if (!FixOriginTimeFlag) {

		/* calculate weighted mean of observed times */

		wtmtx = gauss_par->WtMtrx;
		sum = 0.0L;
		wt_sum = 0.0;

		for (nrow = 0; nrow < num_arrivals; nrow++) {
			if (!arrival[nrow].abs_time)
				continue;	// ignore obs without absolute timing
			wtmtxrow =  wtmtx[nrow];
			for (ncol = 0; ncol < num_arrivals; ncol++) {
				if (!arrival[ncol].abs_time)
					continue;	// ignore obs without absolute timing
				sum += (long double) *(wtmtxrow + ncol) * arrival[ncol].obs_time;
				wt_sum += (double) *(wtmtxrow + ncol);
			}
		}
		if (wt_sum > 0.0)
			weighted_mean = sum / (long double) wt_sum;
		else
			weighted_mean = (long double) arrival[0].obs_time;

	} else {

		/* use fixed origin time as reference */

		weighted_mean = phypo->time;
	}


	/* set centered observed times */

	if (message_flag >= 3) {
		putmsg(3, "");
		putmsg(3, "Delayed, Sorted, Centered Observations:");
	}
	for (narr = 0; narr < num_arrivals; narr++) {
		arrival[narr].obs_centered =
				(double) (arrival[narr].obs_time - weighted_mean);
		if (message_flag >= 3) {
			sprintf(MsgStr,
				"  %3d  %-6s %-6s %2.2d:%2.2d:%7.4lf - %7.4lfs -> %8.4lf (%10.4lf)",
				narr, arrival[narr].label, arrival[narr].phase,
				arrival[narr].hour, arrival[narr].min,
				arrival[narr].sec, arrival[narr].delay, arrival[narr].obs_centered,
				((double) arrival[narr].obs_time));
			putmsg(3, MsgStr);
		}
	}

	gauss_par->meanObs = weighted_mean;

}


/** function to calculate weighted mean of predicted travel times */
/*		(TV82, eq. A-38) */

INLINE void CalcCenteredTimesPred(int num_arrivals, ArrivalDesc *arrival, GaussLocParams* gauss_par)
{

	int nrow, ncol, narr;
	double sum, weighted_mean, pred_time_row;
	DMatrix wtmtx;
	double *wtmtxrow, wt_sum;


	if (!FixOriginTimeFlag) {

		wtmtx = gauss_par->WtMtrx;
		sum = 0.0;
		wt_sum = 0.0;

		for (nrow = 0; nrow < num_arrivals; nrow++) {
			// AJL 20041115 bug fix!
			if (arrival[nrow].pred_travel_time <= 0.0)
				continue;	// ignore obs without predicted times
			// END
			if (!arrival[nrow].abs_time)
				continue;	// ignore obs without absolute timing
			wtmtxrow = wtmtx[nrow];
			pred_time_row = arrival[nrow].pred_travel_time;
			for (ncol = 0; ncol < num_arrivals; ncol++) {
				// AJL 20041115 bug fix!
				if (arrival[ncol].pred_travel_time <= 0.0)
					continue;	// ignore obs without predicted times
				// END
				if (!arrival[ncol].abs_time)
					continue;	// ignore obs without absolute timing
				sum += (double) *(wtmtxrow + ncol) * pred_time_row;
				wt_sum += (double) *(wtmtxrow + ncol);
			}
		}

		if (wt_sum > 0.0)
			weighted_mean = sum / wt_sum;
		else
			weighted_mean = (long double) arrival[0].pred_travel_time;

	} else {

		/* for fixed origin time use travel time directly */
		weighted_mean = 0.0;

	}



	/* set centered predicted times */

	for (narr = 0; narr < num_arrivals; narr++) {
		// AJL 20041115 bug fix!
		if (arrival[narr].pred_travel_time <= 0.0)
			continue;	// ignore obs without predicted times
		// END
		arrival[narr].pred_centered =
				arrival[narr].pred_travel_time - weighted_mean;
	}


	gauss_par->meanPred = (double) weighted_mean;

}



/** function to calculate probability density */
/*		(MEN92, eq. 14) */

INLINE double CalcSolutionQuality(int num_arrivals, ArrivalDesc *arrival,
				  GaussLocParams* gauss_par, int itype, double* pmisfit, double* potime, double* potime_var, double cell_diagonal_time_var, double* pvalue_search)
{
	double value;

	if (LocMethod == METH_GAU_ANALYTIC) {
		value = CalcSolutionQuality_GAU_ANALYTIC(num_arrivals, arrival, gauss_par, itype, pmisfit, potime);
		*pvalue_search = value;
		return (value);
	} else if (LocMethod == METH_GAU_TEST) {
		value = CalcSolutionQuality_GAU_TEST(num_arrivals, arrival, gauss_par, itype, pmisfit, potime);
		*pvalue_search = value;
		return (value);
	} else if (LocMethod == METH_ML_OT) {
		return (CalcSolutionQuality_ML_OT(num_arrivals, arrival,
			gauss_par, itype, pmisfit, potime, potime_var, cell_diagonal_time_var, 0, pvalue_search));
	} else if (LocMethod == METH_EDT) {
		return (CalcSolutionQuality_EDT(num_arrivals, arrival,
			gauss_par, itype, pmisfit, potime, potime_var, cell_diagonal_time_var, 0, pvalue_search));
	} else if (LocMethod == METH_EDT_BOX) {
		return (CalcSolutionQuality_EDT(num_arrivals, arrival,
			gauss_par, itype, pmisfit, potime, potime_var, cell_diagonal_time_var, 1, pvalue_search));
	} else {
		return(-1.0);
	}

}



/** function to calculate probability density */
/*	EDT - sum of probabilities of difference of obs - difference of travel times
		for all pairs of obs
*/

#define EDT_OT_WT_FLOOR log(0.00001)

INLINE double CalcSolutionQuality_EDT(int num_arrivals, ArrivalDesc *arrival,
				      GaussLocParams* gauss_par, int itype, double* pmisfit, double* potime, double* potime_var, double cell_diagonal_time_var, int method_box, double* pvalue_search)
{

	int nrow, ncol;

	long double edt_sum, prob, edtSumMisfit;

	double edt_misfit, edt_weight;
	double weight, weight2;
	double ln_prob_density, rms_misfit;

	DMatrix edtmtx;
	double sigma2_row;
	double obs_minus_pred;

	int no_abs_time_row;

	int icalc_otime = 0;

	// EDT_OT_WT
	int num_otime_error;
	long double ot_row, ot_prob, ot_row_2, ot_error_2;
	long double ot_sum, ot_weight, ot_var, ot_2_sum, ot_var_weight;

	// EDT_OT_WT_ML
	static double *ot_ml_arrival = NULL;	// array of ot estimate for each arrival
	static double *ot_ml_arrival_edt_sum = NULL;	// array of weight of ot estimate for each arrival
	int isize_ot_ml_array = 0;
	double ot_ml = 0.0, ot_ml_var;

	// OT additions 20071220
	double ot_prob_max = 0.0;

	// Gauss2
	double tt_error;


	// method_box
	//double error_row;
	double amp_row, unc_limit;

	// search pdf different from true
	int iuse_cell_diagonal_time_var;
	//double sigma2_row_search = 0.0;
	//double weight_search = 0.0, weight2_search, edt_weight_search;
	//long double prob_search = 0.0L, edt_sum_search = 0.0L, edtSumMisfit_search = 0.0L;


	// initialize
	if (potime != NULL)
		icalc_otime = 1;
	edtmtx = gauss_par->EDTMtrx;

	// check if use_cell_diagonal_time_var
	iuse_cell_diagonal_time_var = 0;
	//if (pvalue_search != NULL && cell_diagonal_time_var > 0.0)
	if (cell_diagonal_time_var > 0.0)
		iuse_cell_diagonal_time_var = 1;

	// check size of EDT_OT_WT_ML static arrays
	if ((EDT_use_otime_weight == 2 || icalc_otime)) {
		if (isize_ot_ml_array < num_arrivals) {
			isize_ot_ml_array = num_arrivals;
			free(ot_ml_arrival);
			if ((ot_ml_arrival = (double *) calloc(isize_ot_ml_array, sizeof(double))) == NULL)
				puterr("ERROR: allocating double storage array for EDT_OT_WT_ML ot_ml_arrival.");
			free(ot_ml_arrival_edt_sum);
			if ((ot_ml_arrival_edt_sum = (double *) calloc(isize_ot_ml_array, sizeof(double))) == NULL)
				puterr("ERROR: allocating double storage array for EDT_OT_WT_ML ot_ml_arrival_edt_sum.");
		}
		for (nrow = 0; nrow < num_arrivals; nrow++)
			ot_ml_arrival_edt_sum[nrow] = 0.0;
	}

	if (icalc_otime) {
		for (nrow = 0; nrow < num_arrivals; nrow++)
			arrival[nrow].weight = 0.0;
	}


	/* calculate weighted mean of predicted travel times  */
	/*		(TV82, eq. A-38) */
	CalcCenteredTimesPred(num_arrivals, arrival, gauss_par); // not used for EDT


	/* calculate EDT prop sum */

	edt_sum = 0.0L;
	edt_weight = 0.0;
	ot_prob = 0.0L;
	ot_row = 0.0L;
	ot_row_2 = 0.0L;
	ot_sum = 0.0L;
	ot_2_sum = 0.0L;
	ot_var = 0.0L;
	ot_var_weight = 0.0L;
	ot_weight = 0.0L;
	ot_error_2 = 0.0L;
	num_otime_error = 0;
	for (nrow = 0; nrow < num_arrivals; nrow++) {

		// AJL 20041115 bug fix!
		if (arrival[nrow].pred_travel_time <= 0.0) {
			// iniitalize EDT_OT_WT_ML values
			if (EDT_use_otime_weight == 2 || icalc_otime) {
				ot_ml_arrival_edt_sum[nrow] = -1.0;
			}
			continue;	// ignore obs without predicted times
		}
		// END

		// set error
		// printf("iUseGauss2 %d\n", iUseGauss2);
		if (iUseGauss2) {
			tt_error = arrival[nrow].pred_travel_time * Gauss2.SigmaTfraction;
			if (tt_error < Gauss2.SigmaTmin)
				tt_error = Gauss2.SigmaTmin;
			if (tt_error > Gauss2.SigmaTmax)
				tt_error = Gauss2.SigmaTmax;
			if (icalc_otime)
				arrival[nrow].tt_error = tt_error;
			//printf("arrival[nrow].pred_travel_time %f\t  tt_error %f\n", arrival[nrow].pred_travel_time, tt_error);
			tt_error *= tt_error;
			edtmtx[nrow][nrow] = arrival[nrow].error * arrival[nrow].error + tt_error;
			sigma2_row = edtmtx[nrow][nrow];
		}
		sigma2_row = edtmtx[nrow][nrow];

		if (iuse_cell_diagonal_time_var)
			sigma2_row += cell_diagonal_time_var;
		/*if (iuse_cell_diagonal_time_var) {
		sigma2_row_search = edtmtx[nrow][nrow] + cell_diagonal_time_var;
	}*/
		//error_row = arrival[nrow].error;
		amp_row = arrival[nrow].amplitude;
		obs_minus_pred = arrival[nrow].obs_centered - arrival[nrow].pred_centered;
		no_abs_time_row = !arrival[nrow].abs_time;
		if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
			ot_ml_arrival[nrow] = arrival[nrow].obs_time - (long double) arrival[nrow].pred_travel_time;
			//ot_ml_arrival_edt_sum[nrow] = 0.0;
			ot_error_2 += sigma2_row;
			num_otime_error++;
		}
		else if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
			ot_prob = 0.0;
			ot_row = arrival[nrow].obs_time - (long double) arrival[nrow].pred_travel_time;
			ot_row_2 = ot_row * ot_row;
			ot_error_2 += sigma2_row;
			num_otime_error++;
		}
		for (ncol = nrow + 1; ncol < num_arrivals; ncol++) {
			// AJL 20041115 bug fix!
			if (arrival[ncol].pred_travel_time <= 0.0)
				continue;	// ignore obs without predicted times
			// END
			// check abslolute timing
			if (no_abs_time_row) {
				if (arrival[ncol].abs_time)	// cannot be same station/inst
					continue;
				if (strcmp(arrival[nrow].label, arrival[ncol].label) != 0
								|| strcmp(arrival[nrow].inst, arrival[ncol].inst) != 0)
					continue;	// not same sta/inst
			}
			// calculate EDT misfit:  (obs1 - obs2) - (pred1 - pred2)
			edt_misfit = (double) (obs_minus_pred + arrival[ncol].pred_centered - arrival[ncol].obs_centered);
			// set error
			if (iUseGauss2) {
				tt_error = arrival[ncol].pred_travel_time * Gauss2.SigmaTfraction;
				if (tt_error < Gauss2.SigmaTmin)
					tt_error = Gauss2.SigmaTmin;
				if (tt_error > Gauss2.SigmaTmax)
					tt_error = Gauss2.SigmaTmax;
				tt_error *= tt_error;
				edtmtx[ncol][ncol] = arrival[ncol].error * arrival[ncol].error + tt_error;
			}
			// calculate probability
			if (method_box) {
				unc_limit = amp_row + arrival[ncol].amplitude;	// sum of mean pick unc for each box
				//unc_limit = error_row + arrival[ncol].error;	// sum of box widths
				prob = fabs(edt_misfit) <= unc_limit ? 1.0 : 0.0;
				weight = amp_row * arrival[ncol].amplitude;	// product of mean pick unc for each box
				weight *= (1.0 - edtmtx[nrow][ncol]);		// correlation coeff
			} else {
				if (iuse_cell_diagonal_time_var)
					weight2 = 1.0 / (sigma2_row + edtmtx[ncol][ncol] + cell_diagonal_time_var);	// sum of errors**2
				else
					weight2 = 1.0 / (sigma2_row + edtmtx[ncol][ncol]);	// sum of errors**2
				prob = exp(-0.5 * edt_misfit * edt_misfit * weight2);
				weight = sqrt(weight2);		// errors factor
				weight *= (1.0 - edtmtx[nrow][ncol]);		// correlation coeff
				/*if (iuse_cell_diagonal_time_var) {	// duplicate above 4 lines
				weight2_search = 1.0 / (sigma2_row_search + edtmtx[ncol][ncol] + cell_diagonal_time_var);	// sum of errors**2
				prob_search = exp(-0.5 * edt_misfit * edt_misfit * weight2_search);
				weight_search = sqrt(weight2_search);		// errors factor
				weight_search *= (1.0 - edtmtx[nrow][ncol]);		// correlation coeff
			}*/
			}
			if (iSetStationDistributionWeights)
				weight *= (arrival[nrow].station_weight + arrival[ncol].station_weight) / 2.0;
			prob *= weight;
			edt_sum += prob;
			edt_weight += weight;
			/*if (iuse_cell_diagonal_time_var) {	// duplicate above 5 lines
			if (iSetStationDistributionWeights)
			weight_search *= (arrival[nrow].station_weight + arrival[ncol].station_weight) / 2.0;
			prob_search *= weight_search;
			edt_sum_search += prob_search;
			edt_weight_search += weight_search;
		}*/
			// accumulate EDT weights
			if (icalc_otime) {
				//arrival[ncol].weight += weight;
				//arrival[nrow].weight += weight;
				arrival[ncol].weight += prob;
				arrival[nrow].weight += prob;
			}
			// otime
			if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
				/*if (iuse_cell_diagonal_time_var)	// ???? TEST
				ot_ml_arrival_edt_sum[nrow] += prob_search;
				else*/
				ot_ml_arrival_edt_sum[ncol] += prob;
				// AJL 20070326 bug fix!
				ot_ml_arrival_edt_sum[nrow] += prob;
			}
			else if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
				ot_prob += prob;
			}
		}
		if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
			ot_sum += ot_prob * ot_row ;
			ot_2_sum += ot_prob * ot_row_2;
			ot_weight += ot_prob;
		}
	}

	// OT_WT methods
	if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML
		// EDT_OT_WT_ML method
		ot_ml = calc_maximum_likelihood_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival,  edtmtx, &ot_ml_var, icalc_otime, &ot_prob_max);
		if (icalc_otime && potime_var != NULL) {
			printf("ot_ml_std %lf\n", sqrt(ot_ml_var));
			*potime_var = ot_ml_var;
		}
		ot_var_weight = -ot_ml_var / (ot_error_2 / (long double) num_otime_error);
		if (ot_var_weight > EDT_OT_WT_FLOOR) {
			if (!EDT_otime_weight_active) {
				EDT_otime_weight_active = 1;
				sprintf(MsgStr, "EDT_otime_weight activated, OT_WT exceeds EDT_OT_WT_FLOOR.");
				putmsg(2, MsgStr);
			}
		} else {
			ot_var_weight = EDT_OT_WT_FLOOR;
		}
	}
	else if (EDT_use_otime_weight == 1 && num_otime_error > 0) {	// EDT_OT_WT
		// EDT_OT_WT method
		ot_var = ot_2_sum / ot_weight - (ot_sum / ot_weight) * (ot_sum / ot_weight);
		if (icalc_otime && potime_var != NULL) {
			printf("ot_ml_std %lf\n", sqrt(ot_var));
			*potime_var = ot_var;
		}
		ot_var_weight = -ot_var / (ot_error_2 / (long double) num_otime_error);
		if (ot_var_weight > EDT_OT_WT_FLOOR) {
			if (!EDT_otime_weight_active) {
				EDT_otime_weight_active = 1;
				sprintf(MsgStr, "EDT_otime_weight activated, OT_WT exceeds EDT_OT_WT_FLOOR.");
				putmsg(2, MsgStr);
			}
		} else {
			ot_var_weight = EDT_OT_WT_FLOOR;
		}
		//edt_sum *= ot_var_weight;
		//edt_weight *= ot_var_weight;
	}

	// avoid numerical problems
	if (edt_sum < SMALL_DOUBLE)
		edt_sum = SMALL_DOUBLE;
	/*if (edt_sum_search < SMALL_DOUBLE)
	edt_sum_search = SMALL_DOUBLE;*/
	// create EDT misfit
	if (edt_weight > 0.0 && num_arrivals > 0) {
		// AJL 20051115 bug fix!
		//edtSumMisfit = -log(edt_sum / edt_weight);
		edtSumMisfit = -log(edt_sum);	// non-normalized sum of edt probs - favors solutions with more readings
		// END
		//edtSumMisfit *= 2.0;
		//edtSumMisfit *= num_arrivals;	// best, give power of N
		//edtSumMisfit *= sqrt(num_arrivals);	// works much better for INGV Italy region scale
		/*if (iuse_cell_diagonal_time_var) { 	// duplicate above lines
		edtSumMisfit_search = -log(edt_sum_search);	// non-normalized sum of edt probs - favors solutions with more readings
	}*/
		// otime
		if (icalc_otime) {
			if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
				*potime = ot_ml;
			}
			else {	// EDT_OT_WT or EDT
				*potime = ot_sum / ot_weight;
			}
			// normalize weights
			//if (iUseGauss2) {
				NormalizeWeights(num_arrivals, arrival);
			//}
		}
	} else {
		edtSumMisfit = VERY_LARGE_DOUBLE;
		//edtSumMisfit = edtSumMisfit_search = VERY_LARGE_DOUBLE;
		if (icalc_otime)
			*potime = VERY_LARGE_DOUBLE;
	}
//printf("edt_sum %lf  edtSumMisfit %lf\n", edt_sum, edtSumMisfit);


	// return misfit or ln(prob density)
	if (itype == GRID_MISFIT) {
		/* convert misfit to rms misfit */
		rms_misfit = sqrt(edtSumMisfit + edt_weight);	// edt_weight term similar to divide by num readings in rms
		*pmisfit = rms_misfit;
		return(rms_misfit);
	} else if (itype == GRID_PROB_DENSITY) {
		ln_prob_density = -edtSumMisfit * num_arrivals; 	// best, give power of N
		//if (EDT_otime_weight_active)
		ln_prob_density += ot_var_weight;
		//ln_prob_density = -0.5 * edtSumMisfit;
		rms_misfit = sqrt(edtSumMisfit + edt_weight);	// edt_weight term similar to divide by num readings in rms
		*pmisfit = rms_misfit;
		/*if (iuse_cell_diagonal_time_var) { 	// duplicate above lines
		*pvalue_search = -edtSumMisfit_search * num_arrivals; 	// no ot_var_weight
	} else {
		*pvalue_search = -edtSumMisfit * num_arrivals; 	// no ot_var_weight
	}*/
		return(ln_prob_density);
	} else {
		return(-1.0);
	}



}






/** function to calculate probability density */
/*	ML_OT - maximum of sum of probabilities in otime domain
*/

#define ML_OT_WT_FLOOR log(0.00001)

INLINE double CalcSolutionQuality_ML_OT(int num_arrivals, ArrivalDesc *arrival,
					GaussLocParams* gauss_par, int itype, double* pmisfit, double* potime, double* potime_var, double cell_diagonal_time_var, int method_box, double* pvalue_search)
{

	int nrow;

	long double edt_sum, prob, edtSumMisfit;

	double edt_weight;
	double weight, weight2;
	double ln_prob_density, rms_misfit;

	DMatrix edtmtx;
	double sigma2_row;
	//double obs_minus_pred;

	int icalc_otime = 0;

	// EDT_OT_WT
	int num_otime_error;
	long double ot_row, ot_prob, ot_row_2, ot_error_2;
	long double ot_sum, ot_weight, ot_var, ot_2_sum, ot_var_weight;

	// EDT_OT_WT_ML
	static double *ot_ml_arrival = NULL;	// array of ot estimate for each arrival
	static double *ot_ml_arrival_edt_sum = NULL;	// array of weight of ot estimate for each arrival
	int isize_ot_ml_array = 0;
	double ot_ml = 0.0, ot_ml_var;

	// OT additions 20071220
	double ot_prob_max = 0.0;

	// Gauss2
	double tt_error;


	// search pdf different from true
	int iuse_cell_diagonal_time_var;
	//double sigma2_row_search = 0.0;
	//double weight_search = 0.0, weight2_search, edt_weight_search;
	//long double prob_search = 0.0L, edt_sum_search = 0.0L, edtSumMisfit_search = 0.0L;


	// initialize
	if (potime != NULL)
		icalc_otime = 1;
	edtmtx = gauss_par->EDTMtrx;

	// check if use_cell_diagonal_time_var
	iuse_cell_diagonal_time_var = 0;
	//if (pvalue_search != NULL && cell_diagonal_time_var > 0.0)
	if (cell_diagonal_time_var > 0.0)
		iuse_cell_diagonal_time_var = 1;

	// check size of EDT_OT_WT_ML static arrays
	if ((EDT_use_otime_weight == 2 || icalc_otime)) {
		if (isize_ot_ml_array < num_arrivals) {
			isize_ot_ml_array = num_arrivals;
			free(ot_ml_arrival);
			if ((ot_ml_arrival = (double *) calloc(isize_ot_ml_array, sizeof(double))) == NULL)
				puterr("ERROR: allocating double storage array for EDT_OT_WT_ML ot_ml_arrival.");
			free(ot_ml_arrival_edt_sum);
			if ((ot_ml_arrival_edt_sum = (double *) calloc(isize_ot_ml_array, sizeof(double))) == NULL)
				puterr("ERROR: allocating double storage array for EDT_OT_WT_ML ot_ml_arrival_edt_sum.");
		}
		for (nrow = 0; nrow < num_arrivals; nrow++)
			ot_ml_arrival_edt_sum[nrow] = 0.0;
	}

	if (icalc_otime) {
		for (nrow = 0; nrow < num_arrivals; nrow++)
			arrival[nrow].weight = 0.0;
	}


	/* calculate weighted mean of predicted travel times  */
	/*		(TV82, eq. A-38) */
	CalcCenteredTimesPred(num_arrivals, arrival, gauss_par); // not used for EDT


	/* calculate EDT prop sum */

	edt_sum = 0.0L;
	edt_weight = 0.0;
	ot_prob = 0.0L;
	ot_row = 0.0L;
	ot_row_2 = 0.0L;
	ot_sum = 0.0L;
	ot_2_sum = 0.0L;
	ot_var = 0.0L;
	ot_var_weight = 0.0L;
	ot_weight = 0.0L;
	ot_error_2 = 0.0L;
	num_otime_error = 0;
	for (nrow = 0; nrow < num_arrivals; nrow++) {

		// AJL 20041115 bug fix!
		if (arrival[nrow].pred_travel_time <= 0.0
				  || !arrival[nrow].abs_time) 	// AJL 20071220
		{
			// iniitalize EDT_OT_WT_ML values
			if (EDT_use_otime_weight == 2 || icalc_otime) {
				ot_ml_arrival_edt_sum[nrow] = -1.0;
			}
			continue;	// ignore obs without predicted times
		}
		// END

		// set error
		// printf("iUseGauss2 %d\n", iUseGauss2);
		if (iUseGauss2) {
			tt_error = arrival[nrow].pred_travel_time * Gauss2.SigmaTfraction;
			if (tt_error < Gauss2.SigmaTmin)
				tt_error = Gauss2.SigmaTmin;
			if (tt_error > Gauss2.SigmaTmax)
				tt_error = Gauss2.SigmaTmax;
			if (icalc_otime)
				arrival[nrow].tt_error = tt_error;
			//printf("arrival[nrow].pred_travel_time %f\t  tt_error %f\n", arrival[nrow].pred_travel_time, tt_error);
			tt_error *= tt_error;
			edtmtx[nrow][nrow] = arrival[nrow].error * arrival[nrow].error + tt_error;
		}
		sigma2_row = edtmtx[nrow][nrow];

		if (iuse_cell_diagonal_time_var)
			sigma2_row += cell_diagonal_time_var;
		/*if (iuse_cell_diagonal_time_var) {
		sigma2_row_search = edtmtx[nrow][nrow] + cell_diagonal_time_var;
	}*/
		//error_row = arrival[nrow].error;
		//obs_minus_pred = arrival[nrow].obs_centered - arrival[nrow].pred_centered;
		if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
			ot_ml_arrival[nrow] = arrival[nrow].obs_time - (long double) arrival[nrow].pred_travel_time;
			//ot_ml_arrival_edt_sum[nrow] = 0.0;
			ot_error_2 += sigma2_row;
			num_otime_error++;
		}
		else if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
			ot_prob = 0.0;
			ot_row = arrival[nrow].obs_time - (long double) arrival[nrow].pred_travel_time;
			ot_row_2 = ot_row * ot_row;
			ot_error_2 += sigma2_row;
			num_otime_error++;
		}

		weight2 = 1.0 / sigma2_row;	// sum of errors**2
		weight = sqrt(weight2);		// errors factor

		if (iSetStationDistributionWeights)
			weight *= arrival[nrow].station_weight;

		prob = weight;
		edt_sum += prob;
		edt_weight += weight;

		// accumulate EDT weights
		if (icalc_otime) {
			arrival[nrow].weight += prob;
		}
		// otime
		if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
			//ot_ml_arrival_edt_sum[nrow] += prob;
			ot_ml_arrival_edt_sum[nrow] = 1.0;	// AJL 20071220 not needed?  redundant with normalisation in
		}
		else if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
			ot_prob += prob;
		}

		if (EDT_use_otime_weight == 1) {	// EDT_OT_WT or EDT
			ot_sum += ot_prob * ot_row ;
			ot_2_sum += ot_prob * ot_row_2;
			ot_weight += ot_prob;
		}
	}

	// OT_WT methods
	if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML
		// EDT_OT_WT_ML method
		ot_ml = calc_maximum_likelihood_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival,  edtmtx, &ot_ml_var, icalc_otime, &ot_prob_max);
		if (icalc_otime && potime_var != NULL) {
			printf("ot_ml_std %lf\n", sqrt(ot_ml_var));
			*potime_var = ot_ml_var;
		}
		ot_var_weight = -ot_ml_var / (ot_error_2 / (long double) num_otime_error);
ot_var_weight = log(ot_prob_max);
		if (1 || ot_var_weight > ML_OT_WT_FLOOR) {
			if (!EDT_otime_weight_active) {
				EDT_otime_weight_active = 1;
				sprintf(MsgStr, "EDT_otime_weight activated, OT_WT exceeds ML_OT_WT_FLOOR.");
				putmsg(2, MsgStr);
			}
		} else {
			ot_var_weight = ML_OT_WT_FLOOR;
		}
	}
	else if (EDT_use_otime_weight == 1 && num_otime_error > 0) {	// EDT_OT_WT
		// EDT_OT_WT method
		ot_var = ot_2_sum / ot_weight - (ot_sum / ot_weight) * (ot_sum / ot_weight);
		if (icalc_otime && potime_var != NULL) {
			printf("ot_ml_std %lf\n", sqrt(ot_var));
			*potime_var = ot_var;
		}
		ot_var_weight = -ot_var / (ot_error_2 / (long double) num_otime_error);
		if (1 || ot_var_weight > ML_OT_WT_FLOOR) {
			if (!EDT_otime_weight_active) {
				EDT_otime_weight_active = 1;
				sprintf(MsgStr, "EDT_otime_weight activated, OT_WT exceeds ML_OT_WT_FLOOR.");
				putmsg(2, MsgStr);
			}
		} else {
			ot_var_weight = ML_OT_WT_FLOOR;
		}
		//edt_sum *= ot_var_weight;
		//edt_weight *= ot_var_weight;
	}

	// avoid numerical problems
	if (edt_sum < SMALL_DOUBLE)
		edt_sum = SMALL_DOUBLE;
	/*if (edt_sum_search < SMALL_DOUBLE)
	edt_sum_search = SMALL_DOUBLE;*/
	// create EDT misfit
	if (edt_weight > 0.0 && num_arrivals > 0) {
		// AJL 20051115 bug fix!
		//edtSumMisfit = -log(edt_sum / edt_weight);
		edtSumMisfit = -log(edt_sum);	// non-normalized sum of edt probs - favors solutions with more readings
		// END
		//edtSumMisfit *= 2.0;
		//edtSumMisfit *= num_arrivals;	// best, give power of N
		//edtSumMisfit *= sqrt(num_arrivals);	// works much better for INGV Italy region scale
		/*if (iuse_cell_diagonal_time_var) { 	// duplicate above lines
		edtSumMisfit_search = -log(edt_sum_search);	// non-normalized sum of edt probs - favors solutions with more readings
	}*/
		// otime
		if (icalc_otime) {
			if (EDT_use_otime_weight == 2 || icalc_otime) {	// EDT_OT_WT_ML or otime
				*potime = ot_ml;
			}
			else {	// EDT_OT_WT or EDT
				*potime = ot_sum / ot_weight;
			}
			// normalize weights
			//if (iUseGauss2) {
			NormalizeWeights(num_arrivals, arrival);
			//}
		}
	} else {
		edtSumMisfit = VERY_LARGE_DOUBLE;
		//edtSumMisfit = edtSumMisfit_search = VERY_LARGE_DOUBLE;
		if (icalc_otime)
			*potime = VERY_LARGE_DOUBLE;
	}
//printf("edt_sum %lf  edtSumMisfit %lf\n", edt_sum, edtSumMisfit);


	// return misfit or ln(prob density)
	if (itype == GRID_MISFIT) {
		/* convert misfit to rms misfit */
		rms_misfit = sqrt(edtSumMisfit + edt_weight);	// edt_weight term similar to divide by num readings in rms
		*pmisfit = rms_misfit;
		return(rms_misfit);
	} else if (itype == GRID_PROB_DENSITY) {
		ln_prob_density = -edtSumMisfit * num_arrivals; 	// best, give power of N
		//if (EDT_otime_weight_active)
		//ln_prob_density += ot_var_weight;
		ln_prob_density = ot_var_weight * num_arrivals;
		//ln_prob_density = -0.5 * edtSumMisfit;
		rms_misfit = sqrt(edtSumMisfit + edt_weight);	// edt_weight term similar to divide by num readings in rms
		*pmisfit = rms_misfit;
		/*if (iuse_cell_diagonal_time_var) { 	// duplicate above lines
		*pvalue_search = -edtSumMisfit_search * num_arrivals; 	// no ot_var_weight
	} else {
		*pvalue_search = -edtSumMisfit * num_arrivals; 	// no ot_var_weight
	}*/
		return(ln_prob_density);
	} else {
		return(-1.0);
	}



}



/** function to calculate origin time based on maximum likelihood peak of a set of ot estimates */

INLINE double calc_maximum_likelihood_ot(double *ot_ml_arrival, double *ot_ml_arrival_edt_sum, int num_arrivals, ArrivalDesc *arrival, DMatrix edtmtx, double *pot_ml_var, int iwrite_errors,
double *pprob_max)
{

	int narr;
	ArrivalDesc *parr;
	double arr_prob_max, prob;
	double ot_arr, ot_arr_prob_max = 0.0;
	double range, step, time, tlimit, prob_max, ot_max_like;
	double edt_matrix_rms_error, edt_matrix_diagonal_sum;


	// find ot estimate with largest likelihood over set of ot's
	arr_prob_max = -1.0;
	edt_matrix_diagonal_sum = 0.0;
	for (narr = 0; narr < num_arrivals; narr++) {
		if (ot_ml_arrival_edt_sum[narr] < 0.0)	// skip ignored arrivals
			continue;
		parr = arrival + narr;
		ot_arr = ot_ml_arrival[narr];
		prob = calc_likelihood_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival, edtmtx, ot_arr);
		// store if largest cumulative ot likelihood
		if (prob > arr_prob_max) {
			arr_prob_max = prob;
			ot_arr_prob_max = ot_arr;
		}
		edt_matrix_diagonal_sum += edtmtx[narr][narr];
	}
	if (iwrite_errors && arr_prob_max < 0.0)
		puterr("ERROR: calc_maximum_likelihood_ot: failed to find arr_prob_max.");

	edt_matrix_rms_error = sqrt(edt_matrix_diagonal_sum / num_arrivals);


	// search numerically for precise ot likelihood peak
	range = 3.0 * edt_matrix_rms_error;
	step = edt_matrix_rms_error / 100.0;
	prob_max = arr_prob_max;
	ot_max_like = ot_arr_prob_max;
	// search increasing time
	time = ot_arr_prob_max;
	tlimit = ot_arr_prob_max + range;
	while ((time = time + step) < tlimit) {
		prob = calc_likelihood_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival, edtmtx, time);
		if (prob < prob_max)
			break;
		prob_max = prob;
		ot_max_like = time;
	}
	if (iwrite_errors && time >= tlimit)
		puterr("ERROR: calc_maximum_likelihood_ot: reched end of increasing-time search limit.");

	// search decreasing time
	time = ot_arr_prob_max;
	tlimit = ot_arr_prob_max - range;
	while ((time = time - step) > tlimit) {
		prob = calc_likelihood_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival, edtmtx, time);
		if (prob < prob_max)
			break;
		prob_max = prob;
		ot_max_like = time;
	}
	if (iwrite_errors && time <= tlimit)
		puterr("ERROR: calc_maximum_likelihood_ot: reched end of decreasing-time search limit.");


	// set prob_max - added AJL 20071220
	*pprob_max = prob_max;

	// get variance
	*pot_ml_var = calc_variance_ot(ot_ml_arrival, ot_ml_arrival_edt_sum, num_arrivals, arrival, edtmtx, ot_max_like);

	return(ot_max_like);


}



/** function to calculate origin time likelihood at at given time */

INLINE double calc_likelihood_ot(double *ot_ml_arrival, double *ot_ml_arrival_edt_sum, int num_arrivals, ArrivalDesc *arrival, DMatrix edtmtx, double time) {

	int narr1;
	ArrivalDesc *parr1;
	double prob, prob_arr1, sigma2, temp;

	prob = 0.0;
	for (narr1 = 0; narr1 < num_arrivals; narr1++) {
		if (ot_ml_arrival_edt_sum[narr1] < 0.0)	// skip ignored arrivals
			continue;
		parr1 = arrival + narr1;
		sigma2 = edtmtx[narr1][narr1];
		temp = ot_ml_arrival[narr1] - time;
		// normal dist (normalized relative) of arrival error around ot est for arrival
		prob_arr1 = exp(-0.5 * temp * temp / sigma2) / sqrt(sigma2);
		// weight by edt_prob for arrival
		prob_arr1 *= ot_ml_arrival_edt_sum[narr1];
		// weight by station distribution
		if (iSetStationDistributionWeights)
			prob_arr1 *= parr1->station_weight;
		// cumulate sum of ot likelihood
		prob += prob_arr1;
	}

	return (prob);


}




/** function to calculate origin time variance for at given time */

INLINE double calc_variance_ot(double *ot_ml_arrival, double *ot_ml_arrival_edt_sum, int num_arrivals, ArrivalDesc *arrival, DMatrix edtmtx, double expectation_time) {

	int narr1;
	double variance_sum, temp, sigma2, weight, weight_sum;


	variance_sum = 0.0;
	weight_sum = 0.0;
	for (narr1 = 0; narr1 < num_arrivals; narr1++) {
		if (ot_ml_arrival_edt_sum[narr1] < 0.0)	// skip ignored arrivals
			continue;
		temp = ot_ml_arrival[narr1] - expectation_time;
		// normal dist (normalized relative) of arrival error around ot est for arrival
		sigma2 = edtmtx[narr1][narr1];
		weight = 1.0 / sqrt(sigma2);
		// weight by edt_prob for arrival
		weight *= ot_ml_arrival_edt_sum[narr1];
		// weight by station distribution
		if (iSetStationDistributionWeights)
			weight *= arrival[narr1].station_weight;
		variance_sum += weight * temp * temp;
		weight_sum += weight;
	}

	return (variance_sum / weight_sum);


}




/** function to normalize arrival weights */

int NormalizeWeights(int num_arrivals, ArrivalDesc *arrival)
{
	int narr;
	//double sigmaT2, corr_len2;
	//int corr_len_nonzero = 1;
	//double dx, dy, dz, dist2;
	double weight_sum;
	//double sta_wt;
	//DMatrix null_mtrx = NULL;
	//SourceDesc *sta1, *sta2;
	//double arrivalWeightMax = -1.0;

	//static DMatrix wt_matrix, edt_matrix = NULL;
	//static int last_matrix_alloc_size = -1;


	// get row weights & sum of weights

	weight_sum = 0.0;
	for (narr = 0; narr < num_arrivals; narr++) {
		weight_sum += arrival[narr].weight;
//printf("row %d col %d: wt_tx(r,c) %f  arr(row)_wt %f   wt_sum %lf\n", nrow, ncol, wt_matrix[nrow][ncol], arrival[nrow].weight, weight_sum);
	}
	for (narr = 0; narr < num_arrivals; narr++) {
		arrival[narr].weight = (double) num_arrivals * arrival[narr].weight / weight_sum;
//printf("observation weight: %s %s %s weight: %lf\n", arrival[nrow].label, arrival[nrow].inst, arrival[nrow].comp, arrival[nrow].weight);
	}
	if (message_flag >= 4) {
		sprintf(MsgStr, "EDT Posterior Weight Matrix sum: %f", weight_sum);
		putmsg(4, MsgStr);
	}


	return(0);

}






/** function to calculate probability density */
/*	sum of individual L2 residual probablities */

INLINE double CalcSolutionQuality_GAU_TEST(int num_arrivals, ArrivalDesc *arrival,
		GaussLocParams* gauss_par, int itype, double* pmisfit, double* potime)
{

	int nrow, ncol, narr;

	double misfit;
	double ln_prob_density, rms_misfit;

	double arr_row_res;
	DMatrix wtmtx;
	double *wtmtxrow;

	double pred_min = VERY_LARGE_DOUBLE, pred_max = -VERY_LARGE_DOUBLE;
	double prob_max = -VERY_LARGE_DOUBLE;
	double tshift, tshift_at_prob_max = 0.0;
	double tstep, tstart, tstop;
	double misfit_min = VERY_LARGE_DOUBLE;
	double misfit_tmp, prob;


	wtmtx = gauss_par->WtMtrx;


	/* calculate weighted mean of predicted travel times  */
	/*		(TV82, eq. A-38) */

	CalcCenteredTimesPred(num_arrivals, arrival, gauss_par);


	/* calculate residuals (TV82, eqs. 10-12, 10-13; MEN92, eq. 15) */

	for (narr = 0; narr < num_arrivals; narr++) {
		// AJL 20041115 bug fix!
		if (arrival[narr].pred_travel_time <= 0.0)
			arrival[narr].cent_resid = 0.0;		// ignore obs without predicted times
		else {
			arrival[narr].cent_resid = arrival[narr].obs_centered - arrival[narr].pred_centered;
			if (arrival[narr].pred_centered < pred_min)
				pred_min = arrival[narr].pred_centered;
			if (arrival[narr].pred_centered > pred_max)
				pred_max = arrival[narr].pred_centered;
		}
	}
//printf("pred_min %lf  pred_max %lf\n", pred_min, pred_max);


	// find maximum prob time shift

	tstep = (pred_max - pred_min) / 10.0;
	tstart = pred_min;
	tstop = pred_max;
	while (tstep > (pred_max - pred_min) / 1000000.0) {
		for (tshift = tstart; tshift <= tstop; tshift += tstep) {
			misfit = 0.0;
			prob = 0.0;
			for (nrow = 0; nrow < num_arrivals; nrow++) {
		// AJL 20041115 bug fix!
				if (arrival[nrow].pred_travel_time <= 0.0)
					continue;	// ignore obs without predicted times
		// END
				wtmtxrow = wtmtx[nrow];
				arr_row_res = arrival[nrow].cent_resid + tshift;
				for (ncol = 0; ncol <= nrow; ncol++) {
		// AJL 20041115 bug fix!
					if (arrival[ncol].pred_travel_time <= 0.0)
						continue;	// ignore obs without predicted times
		// END
					if (ncol != nrow) {
						misfit_tmp = (double) *(wtmtxrow + ncol) *
								arr_row_res * (arrival[ncol].cent_resid + tshift);
						//prob += 2.0 * exp(-misfit_tmp);
						//misfit += 2.0 * misfit_tmp;
					} else {
						misfit_tmp = (double) *(wtmtxrow + ncol) *
								arr_row_res * (arrival[ncol].cent_resid + tshift);
						prob += exp(-0.5 * misfit_tmp);
						misfit += misfit_tmp;
					}
				}
			}
			prob /= num_arrivals;
			if (prob > prob_max) {
			//if (misfit < misfit_min) {
				misfit_min = misfit;
				//misfit_min = -log(prob / num_arrivals);
				prob_max = prob;
				tshift_at_prob_max = tshift;
			}
		}
		tstart = tshift_at_prob_max - tstep;
		tstop = tshift_at_prob_max + tstep;
		tstep /= 10.0;
	}
	misfit = misfit_min;
//printf("tshift_at_prob_max %lf  prob_max %lf\n", tshift_at_prob_max, prob_max);

	// get origin time
	if (potime != NULL)
		*potime = CalcMaxLikeOriginTime(num_arrivals, arrival, gauss_par);

	/* return misfit or ln(prob density) */

	if (itype == GRID_MISFIT) {
		/* convert misfit to rms misfit */
		rms_misfit = sqrt(misfit / num_arrivals);
		*pmisfit = rms_misfit;
		return(rms_misfit);
	} else if (itype == GRID_PROB_DENSITY) {
		//ln_prob_density = -0.5 * misfit;
		ln_prob_density = log(prob_max) * num_arrivals * num_arrivals;
		//rms_misfit = sqrt(misfit / num_arrivals);
		rms_misfit = sqrt(misfit);
		*pmisfit = rms_misfit;
		return(ln_prob_density);
	} else {
		return(-1.0);
	}



}



/** function to calculate probability density */
/*		(MEN92, eq. 14) */

INLINE double CalcSolutionQuality_GAU_ANALYTIC(int num_arrivals, ArrivalDesc *arrival,
		GaussLocParams* gauss_par, int itype, double* pmisfit, double* potime)
{

	int nrow, ncol, narr;
	int narr_misfit;

	double misfit = 0.0;
	double ln_prob_density, rms_misfit;

	double arr_row_res;
	DMatrix wtmtx;
	double *wtmtxrow;

	wtmtx = gauss_par->WtMtrx;


	/* calculate weighted mean of predicted travel times  */
	/*		(TV82, eq. A-38) */

	CalcCenteredTimesPred(num_arrivals, arrival, gauss_par);


	/* calculate residuals (TV82, eqs. 10-12, 10-13; MEN92, eq. 15) */

	for (narr = 0; narr < num_arrivals; narr++) {
		// AJL 20041115 bug fix!
		if (arrival[narr].pred_travel_time <= 0.0)
			arrival[narr].cent_resid = 0.0;		// ignore obs without predicted times
		else
			arrival[narr].cent_resid = arrival[narr].obs_centered - arrival[narr].pred_centered;
	}

	narr_misfit = 0;
	for (nrow = 0; nrow < num_arrivals; nrow++) {
		// AJL 20041115 bug fix!
		if (arrival[nrow].pred_travel_time <= 0.0) {
//printf("IGNORE: %s %s\n", arrival[nrow].label, arrival[nrow].phase);
			continue;	// ignore obs without predicted times
		}
		// END
		if (!arrival[nrow].abs_time)
			continue;
		narr_misfit++;
		wtmtxrow = wtmtx[nrow];
		arr_row_res = arrival[nrow].cent_resid;
		for (ncol = 0; ncol <= nrow; ncol++) {
			// AJL 20041115 bug fix!
			if (arrival[ncol].pred_travel_time <= 0.0)
				continue;	// ignore obs without predicted times
			// END
			if (!arrival[ncol].abs_time)
				continue;
			if (ncol != nrow)
				misfit += 2.0 * (double) *(wtmtxrow + ncol) *
						arr_row_res * arrival[ncol].cent_resid;
			else
				misfit += (double) *(wtmtxrow + ncol) *
						arr_row_res * arrival[ncol].cent_resid;
		}
	}


	if (potime != NULL)
		*potime = CalcMaxLikeOriginTime(num_arrivals, arrival, gauss_par);

	/* return misfit or ln(prob density) */

	if (itype == GRID_MISFIT) {
		/* convert misfit to rms misfit */
		rms_misfit = sqrt(misfit / narr_misfit);
		*pmisfit = rms_misfit;
		return(rms_misfit);
	} else if (itype == GRID_PROB_DENSITY) {
		ln_prob_density = -0.5 * misfit;
		rms_misfit = sqrt(misfit / narr_misfit);
		*pmisfit = rms_misfit;
		return(ln_prob_density);
	} else {
		return(-1.0);
	}



}



/** function to calculate maximum likelihood estimate for origin time */
/*		(MEN92, eq. 19) */

long double CalcMaxLikeOriginTime(int num_arrivals, ArrivalDesc *arrival, GaussLocParams* gauss_par)
{

	DMatrix wtmtx;

	wtmtx = gauss_par->WtMtrx;



/* NOTE: the following (MEN92, eq. 19) is unnecessary

	for (nrow = 0; nrow < num_arrivals; nrow++)
	for (ncol = 0; ncol <= nrow; ncol++) {
	if (ncol != nrow)
	time_est += 2.0L * (long double) wtmtx[nrow][ncol]
	* (arrival[nrow].obs_time -
	(long double) arrival[nrow].pred_travel_time);
	else
	time_est += (long double) wtmtx[nrow][ncol] *
	(arrival[nrow].obs_time -
	(long double) arrival[nrow].pred_travel_time);
}
	return(time_est / (long double) gauss_par->WtMtrxSum);
*/

	return(gauss_par->meanObs - (long double) gauss_par->meanPred);

}



/** function to update arrival probabilistic residuals */

INLINE void UpdateProbabilisticResiduals(int num_arrivals, ArrivalDesc *arrival, double prob)
{

	int narr;


	/* update probabilistc residuals */

	for (narr = 0; narr < num_arrivals; narr++) {
		arrival[narr].pdf_residual_sum += prob *
				arrival[narr].cent_resid;
		arrival[narr].pdf_weight_sum += prob;
	}

}





/** function to calculate confidence intervals and save to file */
/*		(MEN92, eq. 25ff) */

#define N_STEPS_SRCH	101
#define N_STEPS_CONF	11

int CalcConfidenceIntrvl(GridDesc* ptgrid, HypoDesc* phypo, char* filename)
{
	FILE *fpio;
	char fname[FILENAME_MAX];

	int ix, iy, iz;
	int iconf, isrch;
	double srch_level, srch_incr, conf_level, conf_incr, prob_den;
	double srch_sum[N_STEPS_SRCH];
	double contour[N_STEPS_CONF];
	double sum_volume;


	/* write message */
	if (message_flag >= 3) {
		putmsg(3, "");
		putmsg(3, "Calculating confidence intervals over grid...");
	}


	for (isrch = 0; isrch < N_STEPS_SRCH; isrch++)
		srch_sum[isrch] = 0.0;


	/* accumulate approx integral of probability density in search bins */
	/*  and normalize sum of bin values * dx*dy*dz */

	sum_volume = ptgrid->sum * ptgrid->dx * ptgrid->dy * ptgrid->dz;
	phypo->probmax /= sum_volume;
	srch_incr = phypo->probmax / (N_STEPS_SRCH - 1);
	for (ix = 0; ix <  ptgrid->numx; ix++) {
		for (iy = 0; iy <  ptgrid->numy; iy++) {
			for (iz = 0; iz <  ptgrid->numz; iz++) {

				ptgrid->array[ix][iy][iz] = (float) (
						exp((double) ptgrid->array[ix][iy][iz])
					/ sum_volume );
				prob_den = ptgrid->array[ix][iy][iz];
				srch_level = 0.0;
				for (isrch = 0; isrch < N_STEPS_SRCH; isrch++) {
					if (prob_den >= srch_level)
						srch_sum[isrch] += prob_den;
					srch_level += srch_incr;
				}

			}
		}
	}
	ptgrid->sum = 1.0;

	/* normalize by 100% confidence level sum */

	for (isrch = 1; isrch < N_STEPS_SRCH; isrch++)
		srch_sum[isrch] /= srch_sum[0];
	srch_sum[0] = 1.0;


	/* open confidence interval file */

	sprintf(fname, "%s.loc.conf", filename);
	if ((fpio = fopen(fname, "w")) == NULL) {
		puterr("ERROR: opening confidence interval output file.");
		return(-1);
	} else {
		NumFilesOpen++;
	}

	/* find confidence levels and write to file */

	conf_incr = 1.0 / (N_STEPS_CONF - 1);
	conf_level = 1.0;
	iconf = N_STEPS_CONF - 1;
	for (isrch = 0; isrch < N_STEPS_SRCH; isrch++) {
		if (srch_sum[isrch] <= conf_level) {
			contour[iconf] = (double) isrch * srch_incr;
			fprintf(fpio, "%lf C %.2lf\n",
				contour[iconf], conf_level);
			if (--iconf < 0)
				break;
			conf_level -= conf_incr;
		}
	}


	fclose(fpio);
	NumFilesOpen--;

	return(0);

}



/** function to generate sample (scatter) of location PDF */

int GenEventScatterGrid(GridDesc* ptgrid, HypoDesc* phypo, ScatterParams* pscat,
			char* filename)
{
	FILE *fpio;
	char fname[FILENAME_MAX];

	int ix, iy, iz;
	double origx, origy, origz;
	double dx, dy, dz, dvol;
	double xval, yval, zval;
	float fdata[4];
	int tot_npoints = 0;
	double xnpoints;
	double probmax, prob_den;



	/* return if no scatter samples requested */
	if (pscat->npts < 1)
		return(0);

	/* write message */
	if (message_flag >= 3) {
		putmsg(3, "");
		putmsg(3, "Generating event scatter file...");
	}

	/* open scatter file */

	sprintf(fname, "%s.loc.scat", filename);
	if ((fpio = fopen(fname, "w")) == NULL) {
		puterr("ERROR: opening scatter output file.");
		return(-1);
	} else {
		NumFilesOpen++;
	}
	/* skip header record (used later to store number of samples taken) */
	fseek(fpio, 4 * sizeof(float), SEEK_SET);


	/* generate N=Scatter->npts events with prob P=prob_den/probmax at  */
	/*	uniformly-randomly chosen grid locations */

	origx = ptgrid->origx;
	origy = ptgrid->origy;
	origz = ptgrid->origz;
	dx = ptgrid->dx;
	dy = ptgrid->dy;
	dz = ptgrid->dz;
	dvol = dx * dy * dz;
	probmax = phypo->probmax;

	for (ix = 0; ix <  ptgrid->numx; ix++) {
		for (iy = 0; iy <  ptgrid->numy; iy++) {
			for (iz = 0; iz <  ptgrid->numz; iz++) {

				prob_den =  ptgrid->array[ix][iy][iz];

				xnpoints = (double) pscat->npts * dvol * prob_den;

				xval = origx + (double) ix * dx;
				yval = origy + (double) iy * dy;
				zval = origz + (double) iz * dz;

				while (xnpoints > 0.0) {

					if (xnpoints > 1.0 || xnpoints - (double) ((int) xnpoints)
							> get_rand_double(0.0, 1.0)) {
						fdata[0] = xval + get_rand_double(-dx / 2.0, dx / 2.0);
						fdata[1] = yval + get_rand_double(-dy / 2.0, dy / 2.0);
						fdata[2] = zval + get_rand_double(-dz / 2.0, dz / 2.0);
						fdata[3] = prob_den;
						fwrite(fdata, sizeof(float), 4, fpio);
						tot_npoints++;
					}

					xnpoints -= 1.0;

				}

			}
		}
	}


	/* write header informaion */
	fseek(fpio, 0, SEEK_SET);
	fwrite(&tot_npoints, sizeof(int), 1, fpio);
	fdata[0] = (float) probmax;
	fwrite(fdata, sizeof(float), 1, fpio);

	fclose(fpio);
	NumFilesOpen--;

	/* write message */
	if (message_flag >= 3) {
		sprintf(MsgStr, "  %d points generated.", tot_npoints);
		putmsg(3, MsgStr);
		sprintf(MsgStr, "  (%d points requested, dvol= %lf, probmax=%lf)",
			pscat->npts, dvol, probmax);
		putmsg(3, MsgStr);
	}

	return(0);

}



/** function to read input file */

// AJL 20071217 - added char** passing of parameters

int ReadNLLoc_Input(FILE* fp_input, char** param_line_array, int n_param_lines)
{
	int istat, iscan;
	char param[MAXLINE] = "\0", *pchr;
	char line_buf[MAXLINE_LONG];
	char *line;
	char *fgets_return;

	int flag_control = 0, flag_outfile = 0, flag_grid = 0, flag_search = 0,
	flag_method = 0, flag_comment = 0, flag_signature = 0,
	flag_hyptype = 0, flag_gauss = 0, flag_gauss2 = 0, flag_trans = 0, flag_comp = 0,
	flag_phstat = 0, flag_phase_id = 0, flag_sta_wt = 0, flag_qual2err = 0,
	flag_mag = 0, flag_alias = 0, flag_exclude = 0, flag_time_delay = 0,
	flag_topo_surface = 0, flag_time_delay_surface = 0, flag_elev_corr = 0,
	flag_otime = 0, flag_angles = 0, flag_source = 0;
	int flag_include = 1;

	char **param_lines;


	// param lines not needed by NLDiffLoc
	if (nll_mode == MODE_DIFFERENTIAL) {
		flag_search = 1;
	}


	/* read each input line */

	line = line_buf;	// for reading from file case
	param_lines = param_line_array;
	fgets_return = NULL;

	while (
		// reading from file (control file or included file
		(fp_input != NULL && (fgets_return = fgets(line, 4*MAXLINE, fp_input)) != NULL)
		||
		fp_include != NULL
		||
		// reading from string
		(param_line_array != NULL && ((param_lines - param_line_array) < n_param_lines)
		      && (fgets_return = line = *(param_lines++)) != NULL)
	      )
	{

		/* check for end of include file */

		if (fgets_return == NULL && fp_include != NULL) {
			SwapBackIncludeFP(&fp_input);
			continue;
		}

//printf(line);

		istat = -1;

		/*read parameter line */

		if ((iscan = sscanf(line, "%s", param)) < 0 )
			continue;

		/* skip comment line or white space */

		if (strncmp(param, "#", 1) == 0 || isspace(param[0]))
			istat = 0;



		/* read include file params and set input to include file */

		if (strcmp(param, "INCLUDE") == 0)
			if ((istat = GetIncludeFile(strchr(line, ' '), &fp_input)) < 0) {
			puterr("ERROR: processing include file.");
			flag_include = 0;
			}


			/* read control params */

			if (strcmp(param, "CONTROL") == 0) {
				if ((istat = get_control(strchr(line, ' '))) < 0)
					puterr("ERROR: readingng control params.");
				else
					flag_control = 1;
			}


			/* read grid params */

			if (strcmp(param, "LOCGRID") == 0) {
				if ((istat = GetNLLoc_Grid(strchr(line, ' '))) < 0)
					puterr("ERROR: reading grid parameters.");
				else
					flag_grid = 1;
			}


			/* read file names */

			if (strcmp(param, "LOCFILES") == 0) {
				if ((istat = GetNLLoc_Files(strchr(line, ' '))) < 0)
					puterr("ERROR: reading NLLoc output file name.");
				else
					flag_outfile = 1;
			}


			/* read output file types names */

			if (strcmp(param, "LOCHYPOUT") == 0) {
				if ((istat = GetNLLoc_HypOutTypes(strchr(line, ' '))) < 0)
					puterr("ERROR: reading NLLoc hyp output file types.");
				else
					flag_hyptype = 1;
			}


			/* read search type */

			if (strcmp(param, "LOCSEARCH") == 0) {
				if ((istat = GetNLLoc_SearchType(strchr(line, ' '))) < 0)
					puterr("ERROR: reading NLLoc search type.");
				else
					flag_search = 1;
			}


			/* read method */

			if (strcmp(param, "LOCMETH") == 0) {
				if ((istat = GetNLLoc_Method(strchr(line, ' '))) < 0)
					puterr("ERROR: reading NLLoc method.");
				else
					flag_method = 1;
			}


			/* read fixed origin time parameters */

			if (strcmp(param, "LOCFIXOTIME") == 0) {
				if ((istat = GetNLLoc_FixOriginTime(
								 strchr(line, ' '))) < 0)
					puterr("ERROR: reading NLLoc fixed origin time params.");
				else
					flag_otime = 1;
			}


			/* read phase identifier values */

			if (strcmp(param, "LOCPHASEID") == 0) {
				if ((istat = GetPhaseID(strchr(line, ' '))) < 0)
					puterr("ERROR: reading phase identifier values.");
				else
					flag_phase_id = 1;
			}


			/* read station distance weighting values */

			if (strcmp(param, "LOCSTAWT") == 0) {
				if ((istat = GetStaWeight(strchr(line, ' '))) < 0)
					puterr("ERROR: reading station distance weighting values.");
				else
					flag_sta_wt = 1;
			}


			/* read quality2error values */

			if (strcmp(param, "LOCQUAL2ERR") == 0) {
				if ((istat = GetQuality2Err(strchr(line, ' '))) < 0)
					puterr("ERROR: reading quality2error values.");
				else
					flag_qual2err = 1;
			}


			/* read comment */

			if (strcmp(param, "LOCCOM") == 0) {
				strcpy(Hypocenter.comment, strchr(line, ' ') + 1);
				     //*(strchr(Hypocenter.comment, '\n')) = '\0';
				TrimString(Hypocenter.comment);
				sprintf(MsgStr, "LOCCOMMENT:  %s\n", Hypocenter.comment);
				putmsg(3, MsgStr);
				flag_comment = 1;
			}


			/* read signature */

			if (strcmp(param, "LOCSIG") == 0) {
				strcpy(LocSignature, strchr(line, ' ') + 1);
				     //*(strchr(LocSignature, '\n')) = '\0';
				TrimString(LocSignature);
				sprintf(MsgStr, "LOCSIGNATURE:  %s\n",
					LocSignature);
				putmsg(3, MsgStr);
				flag_signature = 1;
			}


			/* read gauss params */

			if (strcmp(param, "LOCGAU2") == 0) {
				if ((istat = GetNLLoc_Gaussian2(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Gaussian2 parameters.");
				else
					flag_gauss2 = 1;
			}


			/* read gauss params */

			if (strcmp(param, "LOCGAU") == 0) {
				if ((istat = GetNLLoc_Gaussian(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Gaussian parameters.");
				else
					flag_gauss = 1;
			}



			/* read phase statistics params */

			if (strcmp(param, "LOCPHSTAT") == 0) {
				if ((istat = GetNLLoc_PhaseStats(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Phase Statistics parameters.");
				else
					flag_phstat = 1;
			}


			/* read take-off angles params */

			if (strcmp(param, "LOCANGLES") == 0) {
				if ((istat = GetNLLoc_Angles(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Take-off Angles parameters.");
				else
					flag_angles = 1;
			}


			/* read magnitude calculation params */

			if (strcmp(param, "LOCMAG") == 0) {
				if ((istat = GetNLLoc_Magnitude(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Magnitude Calculation parameters.");
				else
					flag_mag = 1;
			}


			/* read component params */

			if (strcmp(param, "LOCCMP") == 0) {
				if ((istat = GetCompDesc(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Component description parameters.");
				else
					flag_comp = 1;
			}


			/* read alias params */

			if (strcmp(param, "LOCALIAS") == 0) {
				if ((istat = GetLocAlias(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Alias parameters.");
				else
					flag_alias = 1;
			}


			/* read exclude params */

			if (strcmp(param, "LOCEXCLUDE") == 0) {
				if ((istat = GetLocExclude(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Exclude parameters.");
				else
					flag_exclude = 1;
			}


			/* read station time delay params */

			if (strcmp(param, "LOCDELAY") == 0) {
				if ((istat = GetTimeDelays(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Time Delay parameters.");
				else
					flag_time_delay = 1;
			}


			/* read surface time delay params */

			if (strcmp(param, "LOCTOPO_SURFACE") == 0) {
				if ((istat = GetTopoSurface(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Topo Surface parameters.");
				else
					flag_topo_surface = 1;
			}


			/* read surface time delay params */

			if (strcmp(param, "LOCDELAY_SURFACE") == 0) {
				if ((istat = GetTimeDelaySurface(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Time Delay Surface parameters.");
				else
					flag_time_delay_surface = 1;
			}


			/* read station elevation correction params */

			if (strcmp(param, "LOCELEVCORR") == 0) {
				if ((istat = GetElevCorr(strchr(line, ' '))) < 0)
					puterr("ERROR: reading Elevation Correction parameters.");
				else
					flag_elev_corr = 1;
			}


			/* read source params */

			if (strcmp(param, "LOCSRCE") == 0 || strcmp(param, "GTSRCE") == 0) {
				if ((istat = GetNextSource(strchr(line, ' '))) < 0) {
					puterr("ERROR: reading source params:");
					puterr(line);
				} else
					flag_source = 1;
			}



			/*read transform params */

			if (strcmp(param, "TRANS") == 0) {
				if ((istat = get_transform(0, strchr(line, ' '))) < 0)
					puterr("ERROR: reading transformation parameters.");
				else
					flag_trans = 1;
			}



			/* unrecognized input */

			if (istat < 0) {
				if ((pchr = strchr(line, '\n')) != NULL)
					*pchr = '\0';
				sprintf(MsgStr, "Skipping input: %s", line);
				putmsg(5, MsgStr);
			}

	}


	/* check for missing required input */

	if (!flag_control)
		puterr("ERROR: reading control (CONTROL) params.");
	if (!flag_outfile)
		puterr("ERROR: reading i/o file (LOCFILES) params.");
	if (!flag_trans)
		puterr("ERROR: reading transformation (TRANS) params.");
	if (!flag_grid)
		puterr("ERROR: reading grid (LOCGRID) params.");
	if (!flag_search)
		puterr("ERROR: reading search type (LOCSEARCH) params.");
	if (!flag_method)
		puterr("ERROR: reading method (LOCMETH) params.");
	if (!flag_gauss)
		puterr("ERROR: reading Gaussian (LOCGAU) params.");
	if (!flag_qual2err)
		puterr("ERROR: reading Quality2Error (LOCQUAL2ERR) params.");


	/* check for missing optional input */

	if (!flag_gauss) {
		sprintf(MsgStr, "INFO: no Gaussian2 (LOCGAU2) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_comment) {
		sprintf(MsgStr, "INFO: no comment (LOCCOM) params read.");
		putmsg(2, MsgStr);
		Hypocenter.comment[0] = '\0';
	}
	if (!flag_signature) {
		sprintf(MsgStr, "INFO: no signature (LOCSIG) params read.");
		putmsg(2, MsgStr);
		LocSignature[0] = '\0';
	}
	if (!flag_hyptype) {
		sprintf(MsgStr,
			"INFO: no hypocenter output file type (LOCHYPOUT) params read.");
		putmsg(2, MsgStr);
		sprintf(MsgStr,
			"INFO: DEFAULT: \"LOCHYPOUT SAVE_NLLOC_ALL SAVE_HYPOINVERSE_Y2000_ARC\"");
		putmsg(2, MsgStr);
		iSaveNLLocEvent = iSaveNLLocSum = 1;
		iSaveHypo71Sum = iSaveHypoEllSum = 0;
		iSaveHypo71Event = iSaveHypoEllEvent = 0;
		iSaveHypoInvSum = 0;
		iSaveHypoInvY2KArc = 1;
		iSaveAlberto4Sum = 0;
		iSaveSnapSum = 0;
		iSaveDecSec = 0;
		iSaveNone = 0;
	}
	if (!flag_phase_id) {
		sprintf(MsgStr, "INFO: no phase identifier (LOCPHASEID) values read.");
		putmsg(2, MsgStr);
	}
	if (!flag_sta_wt) {
		sprintf(MsgStr, "INFO: no station distance weighting (LOCSTAWT) values read.");
		putmsg(2, MsgStr);
	}
	if (!flag_mag) {
		sprintf(MsgStr, "INFO: no Magnitude Calculation (LOCMAG) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_phstat) {
		sprintf(MsgStr,
			"INFO: no PhaseStatistics (LOCPHSTAT) params read.");
		putmsg(2, MsgStr);
		RMS_Max = VERY_LARGE_DOUBLE;
		NRdgs_Min = -1;
		Gap_Max = VERY_LARGE_DOUBLE;
		P_ResidualMax = VERY_LARGE_DOUBLE;
		S_ResidualMax = VERY_LARGE_DOUBLE;
		Ell_Len3_Max = VERY_LARGE_DOUBLE;
		Hypo_Depth_Min = -VERY_LARGE_DOUBLE;
		Hypo_Depth_Max = VERY_LARGE_DOUBLE;
	}
	if (!flag_comp) {
		sprintf(MsgStr, "INFO: no Component Descirption (LOCCMP) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_alias) {
		sprintf(MsgStr, "INFO: no Alias (LOCALIAS) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_exclude) {
		sprintf(MsgStr, "INFO: no Exclude (LOCEXCLUDE) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_time_delay) {
		sprintf(MsgStr, "INFO: no Time Delay (LOCDELAY) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_topo_surface) {
		sprintf(MsgStr, "INFO: no Topo Surface (LOCTOPO_SURFACE) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_time_delay_surface) {
		sprintf(MsgStr, "INFO: no Time Delay Surface (LOCDELAY_SURFACE) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_elev_corr) {
		sprintf(MsgStr, "INFO: no Elevation Correction (LOCELEVCORR) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_otime) {
		sprintf(MsgStr, "INFO: no Fixed Origin Time (LOCFIXOTIME) params read.");
		putmsg(2, MsgStr);
	}
	if (!flag_angles) {
		sprintf(MsgStr, "INFO: no Take-off Angles (LOCANGLES) params read,");
		putmsg(2, MsgStr);
		sprintf(MsgStr, "      default is angleMode=ANGLES_NO, qualtiyMin=5 .");
		putmsg(2, MsgStr);
		angleMode = ANGLE_MODE_NO;
		iAngleQualityMin = 5;
	}
	if (!flag_source) {
		sprintf(MsgStr, "INFO: no Station (LOCSRCE or GTSRCE) params read.");
		putmsg(2, MsgStr);
	}



	return (flag_include * flag_control * flag_outfile * flag_grid *
			flag_search *
			flag_method * flag_gauss * flag_qual2err * flag_trans - 1);
}



/** function to read output file name ***/

int GetNLLoc_Files(char* line1)
{
	int istat, nObsFile;
	char fnobs[FILENAME_MAX];

	istat = sscanf(line1, "%s %s %s %s %d", fnobs, ftype_obs, fn_loc_grids,
		       fn_path_output, &iSwapBytesOnInput);
	if (istat < 5)
		iSwapBytesOnInput = 0;

	/* check for wildcards in observation file name */
	NumObsFiles = ExpandWildCards(fnobs, fn_loc_obs, MAX_NUM_OBS_FILES);

	if (message_flag >= 3) {
		sprintf(MsgStr,
			"LOCFILES:  ObsType: %s  InGrids: %s.*  OutPut: %s.* iSwapBytesOnInput: %d",
			ftype_obs, fn_loc_grids, fn_path_output, iSwapBytesOnInput);
		putmsg(3, MsgStr);
		for (nObsFile = 0; nObsFile < NumObsFiles; nObsFile++) {
			sprintf(MsgStr,
				"   Obs File: %3d  %s", nObsFile, fn_loc_obs[nObsFile]);
			putmsg(3, MsgStr);
		}
	}

	if (NumObsFiles == MAX_NUM_OBS_FILES)
		putmsg(1, "LOCFILES: WARNING: maximum number of files/events reached");

	return(0);
}




/** function to read search type ***/

int GetNLLoc_SearchType(char* line1)
{
	int istat, ierr;

	char search_type[MAXLINE];


	istat = sscanf(line1, "%s", search_type);

	if (istat != 1)
		return(-1);

	if (strcmp(search_type, "GRID") == 0) {

		SearchType = SEARCH_GRID;
		istat = sscanf(line1, "%s %d", search_type, &(Scatter.npts));
		if (istat != 2)
			return(-1);

		sprintf(MsgStr, "LOCSEARCH:  Type: %s NumScatter %d",
			search_type, Scatter.npts);
		putmsg(3, MsgStr);

	} else if (strcmp(search_type, "MET") == 0) {

		SearchType = SEARCH_MET;
		istat = sscanf(line1, "%s %d %d %d %d %d %lf %lf %lf %lf",
			       search_type, &MetNumSamples, &MetLearn, &MetEquil,
			       &MetStartSave, &MetSkip,
			       &MetStepInit, &MetStepMin, &MetStepFact, &MetProbMin);
		ierr = 0;

		sprintf(MsgStr,
			"LOCSEARCH:  Type: %s  numSamples %d  numLearn %d  numEquilibrate %d  startSave %d  numSkip %d  stepInit %lf  stepMin %lf  stepFact %lf  probMin %lf",
			search_type, MetNumSamples, MetLearn, MetEquil,
			MetStartSave, MetSkip,
			MetStepInit, MetStepMin, MetStepFact, MetProbMin);
		putmsg(3, MsgStr);

		if (checkRangeInt("LOCSEARCH", "numSamples", MetNumSamples, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "numLearn", MetLearn, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "numEquilibrate", MetEquil, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "startSave", MetStartSave, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "numSkip", MetSkip, 1, 1, 0, 0) != 0)
			ierr = -1;
		if (checkRangeDouble("LOCSEARCH", "stepMin", MetStepMin, 1, 0.0, 0, 0.0) != 0)
			ierr = -1;
		if (ierr < 0)
			return(-1);
		if (istat != 10)
			return(-1);

		//?? AJL 17JAN2000 MetUse = MetNumSamples - MetEquil;
		MetUse = MetNumSamples - MetStartSave;

		/* check for "normal" StartSave value*/
		if (MetStartSave < MetLearn + MetEquil) {
			sprintf(MsgStr,
				"LOCSEARCH:  WARNING: Metropolis StartSave < NumLearn + NumEquilibrate.");
			putmsg(1, MsgStr);
		}

	} else if (strcmp(search_type, "OCT") == 0) {

		SearchType = SEARCH_OCTTREE;
		istat = sscanf(line1, "%s %d %d %d %lf %d %d %d %d %lf",
			       search_type, &octtreeParams.init_num_cells_x,
			       &octtreeParams.init_num_cells_y, &octtreeParams.init_num_cells_z,
			       &octtreeParams.min_node_size, &octtreeParams.max_num_nodes,
			       &octtreeParams.num_scatter, &octtreeParams.use_stations_density,
			       &octtreeParams.stop_on_min_node_size,
			       &octtreeParams.mean_cell_velocity);

		if (istat < 8)
			octtreeParams.use_stations_density = 0;
		if (octtreeParams.use_stations_density < 0)
			octtreeParams.use_stations_density = 0;

		if (istat < 9)
			octtreeParams.stop_on_min_node_size = 1;

		if (istat < 10)
			octtreeParams.mean_cell_velocity = -1.0;


		sprintf(MsgStr,
			"LOCSEARCH:  Type: %s  init_num_cells_x %d  init_num_cells_y %d  init_num_cells_z %d  min_node_size %f  max_num_nodes %d  num_scatter %d  use_stations_density %d  stop_on_min_node_size %d  octtreeParams.mean_cell_velocity %f",
			search_type, octtreeParams.init_num_cells_x, octtreeParams.init_num_cells_y,
			octtreeParams.init_num_cells_z,
			octtreeParams.min_node_size, octtreeParams.max_num_nodes,
			octtreeParams.num_scatter, octtreeParams.use_stations_density,
			octtreeParams.stop_on_min_node_size, octtreeParams.mean_cell_velocity);
		putmsg(3, MsgStr);


		ierr = 0;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_x",
		    octtreeParams.init_num_cells_x, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_y",
		    octtreeParams.init_num_cells_y, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_z",
		    octtreeParams.init_num_cells_z, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeDouble("LOCSEARCH", "min_node_size",
		    octtreeParams.min_node_size, 1, 0.0, 0, 0.0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "max_num_nodes",
		    octtreeParams.max_num_nodes, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "num_scatter",
		    octtreeParams.num_scatter, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (ierr < 0)
			return(-1);
		if (istat < 7)
			return(-1);

	}


	return(0);
}




/** function to read requested hypocenter output file types ***/

int GetNLLoc_HypOutTypes(char* line1)
{
	int istat;

	char *pchr, hyp_type[MAXLINE];


	sprintf(MsgStr, "LOCHYPOUT:  ");

	/*  20060619 AJL - commented out to allow repeating of LOCHYPOUT
	iSaveNLLocEvent = iSaveNLLocSum = iSaveHypo71Event = iSaveHypo71Sum
	= iSaveHypoEllEvent = iSaveHypoEllSum
	= iSaveHypoInvSum = iSaveHypoInvY2KArc
	= iSaveAlberto4Sum = iSaveNLLocOctree = 0;
	*/

	pchr = line1;
	do {

		/* check for blank line */
		while (*pchr == ' ')
			pchr++;
		if (isspace(*pchr))
			break;

		if ((istat = sscanf(pchr, "%s", hyp_type)) != 1)
			return(-1);

		if (strcmp(hyp_type, "SAVE_NLLOC_ALL") == 0)
			iSaveNLLocEvent = iSaveNLLocSum = 1;
		else if (strcmp(hyp_type, "SAVE_NLLOC_SUM") == 0)
			iSaveNLLocSum = 1;
		else if (strcmp(hyp_type, "SAVE_NLLOC_OCTREE") == 0)
			iSaveNLLocOctree = 1;
		else if (strcmp(hyp_type, "SAVE_HYPO71_ALL") == 0)
			iSaveHypo71Event = iSaveHypo71Sum = 1;
		else if (strcmp(hyp_type, "SAVE_HYPO71_SUM") == 0)
			iSaveHypo71Sum = 1;
		else if (strcmp(hyp_type, "SAVE_HYPOELL_ALL") == 0)
			iSaveHypoEllEvent = iSaveHypoEllSum = 1;
		else if (strcmp(hyp_type, "SAVE_HYPOELL_SUM") == 0)
			iSaveHypoEllSum = 1;
		else if (strcmp(hyp_type, "SAVE_HYPOINV_SUM") == 0)
			iSaveHypoInvSum = 1;
		else if (strcmp(hyp_type, "SAVE_HYPOINVERSE_Y2000_ARC") == 0)
			iSaveHypoInvY2KArc = 1;
		else if (strcmp(hyp_type, "SAVE_ALBERTO_3D_4") == 0)
			iSaveAlberto4Sum = 1;
		/* SH 02/26/2004 added SNAP summary file */
		else if (strcmp(hyp_type, "SAVE_SNAP_SUM") == 0)
			iSaveSnapSum = 1;
		/* filename format, int sec or 5.2 decimal seconds */
		else if (strcmp(hyp_type, "FILENAME_DEC_SEC") == 0)
			iSaveDecSec = 1;
		/* new values NLL PHASE_2 format*/
		/* 20060629 AJL - Added */
		else if (strcmp(hyp_type, "NLL_FORMAT_VER_2") == 0)
			PhaseFormat = FORMAT_PHASE_2;
		else if (strcmp(hyp_type, "NONE") == 0)
		{
			iSaveNone = 1;
			iSaveNLLocEvent = iSaveNLLocSum = iSaveHypo71Sum = iSaveHypoEllSum =
			iSaveHypo71Event = iSaveHypoEllEvent = iSaveHypoInvSum = iSaveHypoInvY2KArc =
			iSaveAlberto4Sum = iSaveSnapSum = iSaveDecSec = 0;
		}
		else
			return(-1);

		strcat(MsgStr, hyp_type);
		strcat(MsgStr, " ");

	} while ((pchr = strchr(pchr + 1, ' ')) != NULL);

	putmsg(3, MsgStr);

	return(0);
}




/** function to read method ***/

int GetNLLoc_Method(char* line1)
{
	int istat, ierr;

	char loc_method[MAXLINE];


	istat = sscanf(line1, "%s %lf %d %d %d %lf %d %lf %d", loc_method,
		       &DistStaGridMax, &MinNumArrLoc, &MaxNumArrLoc, &MinNumSArrLoc,
		       &VpVsRatio, &MaxNum3DGridMemory, &DistStaGridMin, &iRejectDuplicateArrivals);
	if (istat < 8)
		DistStaGridMin = -1.0;
	if (istat < 9)
		iRejectDuplicateArrivals = 1;

	sprintf(MsgStr,
		"LOCMETHOD:  method: %s  minDistStaGrid: %lf  maxDistStaGrid: %lf  minNumberPhases: %d  maxNumberPhases: %d  minNumberSphases: %d  VpVsRatio: %lf  max3DGridMemory: %d  DistStaGridMin: %f  iRejectDuplicateArrivals: %d",
		loc_method, DistStaGridMin, DistStaGridMax, MinNumArrLoc, MaxNumArrLoc,
		MinNumSArrLoc, VpVsRatio, MaxNum3DGridMemory, DistStaGridMin, iRejectDuplicateArrivals);
	putmsg(3, MsgStr);

	ierr = 0;

	if (ierr < 0 || istat < 7)
		return(-1);

	EDT_use_otime_weight = 0;

	if (strcmp(loc_method, "GAU_ANALYTIC") == 0) {
		LocMethod = METH_GAU_ANALYTIC;
	} else if (strcmp(loc_method, "GAU_TEST") == 0) {
		LocMethod = METH_GAU_TEST;
	} else if (strcmp(loc_method, "ML_OT") == 0) {
		LocMethod = METH_ML_OT;
		EDT_use_otime_weight = 2;
	} else if (strcmp(loc_method, "EDT") == 0 || strcmp(loc_method, "EDT_TEST") == 0) {
		LocMethod = METH_EDT;
	} else if (strcmp(loc_method, "EDT_OT_WT") == 0) {
		LocMethod = METH_EDT;
		EDT_use_otime_weight = 1;
	} else if (strcmp(loc_method, "EDT_OT_WT_ML") == 0) {
		LocMethod = METH_EDT;
		EDT_use_otime_weight = 2;
	} else if (strcmp(loc_method, "EDT_BOX") == 0) {
		LocMethod = METH_EDT_BOX;
	} else {
		LocMethod = METH_UNDEF;
		puterr2("ERROR: unrecognized location method:", loc_method);
	}

		if (MaxNumArrLoc < 1)
			MaxNumArrLoc = MAX_NUM_ARRIVALS;

		if (VpVsRatio > 0.0 && GeometryMode == MODE_GLOBAL) {
			puterr("ERROR: cannot use VpVsRatio>0 with TRANSFORM GLOBAL.");
			return(EXIT_ERROR_LOCATE);
		}

		return(0);
}



/** function to read fixed origin time parameters ***/

int GetNLLoc_FixOriginTime(char* line1)
{
	int istat;


	istat = sscanf(line1, "%d %d %d %d %d %lf",
		       &Hypocenter.year, &Hypocenter.month, &Hypocenter.day,
		       &Hypocenter.hour, &Hypocenter.min, &Hypocenter.sec);

	sprintf(MsgStr,
		"LOCFIXOTIME:  %4.4d%2.2d%2.2d %2.2d%2.2d %5.2lf",
		Hypocenter.year, Hypocenter.month, Hypocenter.day,
		Hypocenter.hour, Hypocenter.min, Hypocenter.sec);
	putmsg(3, MsgStr);

	if (istat != 6)
		return(-1);

	FixOriginTimeFlag = 1;

	return(0);
}




/** function to read grid params */

int GetNLLoc_Grid(char* input_line)
{
	int istat;
	char str_save[20];

	istat = sscanf(input_line, "%d %d %d %lf %lf %lf %lf %lf %lf %s %s",
		       &(grid_in.numx), &(grid_in.numy), &(grid_in.numz),
		       &(grid_in.origx), &(grid_in.origy), &(grid_in.origz),
		       &(grid_in.dx), &(grid_in.dy), &(grid_in.dz), grid_in.chr_type,
		       str_save);

	convert_grid_type(&grid_in, 1);
	if (message_flag >= 2)
		display_grid_param(&grid_in);
	sprintf(MsgStr, "LOCGRID: Save: %s", str_save);
	putmsg(3, MsgStr);

	if (istat != 11)
		return(-1);

	if (NumLocGrids < MAX_NUM_LOCATION_GRIDS) {
		LocGrid[NumLocGrids] = grid_in;
		LocGrid[NumLocGrids].autox = 0;
		LocGrid[NumLocGrids].autoy = 0;
		LocGrid[NumLocGrids].autoz = 0;
		if (LocGrid[NumLocGrids].origx < -LARGE_DOUBLE)
			LocGrid[NumLocGrids].autox = 1;
		if (LocGrid[NumLocGrids].origy < -LARGE_DOUBLE)
			LocGrid[NumLocGrids].autoy = 1;
		if (LocGrid[NumLocGrids].origz < -LARGE_DOUBLE)
			LocGrid[NumLocGrids].autoz = 1;
		if (strcmp(str_save, "SAVE") == 0)
			LocGridSave[NumLocGrids] = 1;
		else
			LocGridSave[NumLocGrids] = 0;
		NumLocGrids++;
	} else
		puterr("WARNING: maximum number of location grids exceeded.");

		return(0);
}



/** function to read phase identification values ***/

		int GetPhaseID(char* line1)
{
	int istat, ilen;
	char *substr, *cpos;


	if (NumPhaseID >= MAX_NUM_PHASE_ID) {
		puterr(
				"LOCPHASEID: WARNING: maximum number of PhaseIDs reached, ignoring phase ID.");
		return(-1);
	}

	if ((istat = sscanf(line1, "%s", PhaseID[NumPhaseID].phase)) != 1)
		return(-1);

	substr = strstr(line1, PhaseID[NumPhaseID].phase);

	/* save phase id values with spaces at each end */
	if ((cpos = strchr(substr, '\n')) != NULL)
		*cpos = '\0';
	sprintf(PhaseID[NumPhaseID].id_string, " %s ", substr + 1);

	if ((ilen = strlen(PhaseID[NumPhaseID].id_string)) == 0)
		return(-1);

	sprintf(MsgStr, "LOCPHASEID:");
	putmsg(3, MsgStr);
	sprintf(MsgStr, "  Phase: %s  PhaseID: <%s>",
		PhaseID[NumPhaseID].phase,
		PhaseID[NumPhaseID].id_string);
	putmsg(3, MsgStr);

	NumPhaseID++;

	return(0);
}




/** function to read station distance weighting params ***/

int GetStaWeight(char* line1)
{
	int istat, ierr;


	istat = sscanf(line1, "%d %lf", &iSetStationDistributionWeights, &stationDistributionWeightCutoff);

	sprintf(MsgStr, "LOCSTAWT:  flag: %d  CutoffDist: %f",
		iSetStationDistributionWeights, stationDistributionWeightCutoff);
	putmsg(3, MsgStr);

	ierr = 0;
	//if (checkRangeDouble("LOCSTAWT", "Station distribution weight cutoff distance",
	//		stationDistributionWeightCutoff, 1, 0.0, 0, 0.0) != 0)
	//	ierr = -1;

	if (ierr < 0 || istat != 2)
		return(-1);

	return(0);

}



/** function to read gaussian params ***/

int GetNLLoc_Gaussian2(char* line1)
{
	int istat, ierr;


	istat = sscanf(line1, "%lf %lf %lf", &(Gauss2.SigmaTfraction), &(Gauss2.SigmaTmin), &(Gauss2.SigmaTmax));

	sprintf(MsgStr, "LOCGAUSS2:  SigmaTfraction: %lf  SigmaTmin: %lf  SigmaTmax: %lf",
		Gauss2.SigmaTfraction, Gauss2.SigmaTmin, Gauss2.SigmaTmax);
	putmsg(1, MsgStr);
	//putmsg(3, MsgStr);

	ierr = 0;
	if (checkRangeDouble("LOCGAU2", "SigmaTfraction",
	    Gauss2.SigmaTfraction, 1, 0.0, 1, 1.0) != 0)
		ierr = -1;
	if (checkRangeDouble("LOCGAU2", "SigmaTmin",
	    Gauss2.SigmaTmin, 1, 0.0, 0, 0.0) != 0)
		ierr = -1;
	if (checkRangeDouble("LOCGAU2", "SigmaTmax",
	    Gauss2.SigmaTmax, 1, 0.0, 0, 0.0) != 0)
		ierr = -1;

	if (ierr < 0 || istat != 3)
		return(-1);

	iUseGauss2 = 1;

	return(0);

}



/** function to read gaussian params ***/

int GetNLLoc_Gaussian(char* line1)
{
	int istat, ierr;


	istat = sscanf(line1, "%lf %lf", &(Gauss.SigmaT), &(Gauss.CorrLen));

	sprintf(MsgStr, "LOCGAUSS:  SigmaT: %lf  CorrLen: %lf",
		Gauss.SigmaT, Gauss.CorrLen);
	putmsg(3, MsgStr);

	ierr = 0;
	if (checkRangeDouble("LOCGAU", "SigmaT",
	    Gauss.SigmaT, 1, 0.0, 0, 0.0) != 0)
		ierr = -1;
	if (checkRangeDouble("LOCGAU", "CorrLen",
	    Gauss.CorrLen, 1, 0.0, 0, 0.0) != 0)
		ierr = -1;

	if (ierr < 0 || istat != 2)
		return(-1);

	return(0);
}





/** function to read magnitude calculation type ***/

int GetNLLoc_Magnitude(char* line1)
{
	int istat, ierr;

	char mag_type[MAXLINE];

	if (NumMagnitudeMethods >= MAX_NUM_MAG_METHODS) {
		puterr2("ERROR: maximum number of LOCMAG statements read: ignoring: ", line1);
		return(-1);
	}

	istat = sscanf(line1, "%s", mag_type);

	if (istat != 1)
		return(-1);

	if (strcmp(mag_type, "ML_HB") == 0) {

		// default values
		Magnitude[NumMagnitudeMethods].hb_Ro = 100.0;
		Magnitude[NumMagnitudeMethods].hb_Mo = 3.0;

		Magnitude[NumMagnitudeMethods].type = MAG_ML_HB;
		istat = sscanf(line1, "%s %lf %lf %lf %lf %lf",
			       mag_type, &(Magnitude[NumMagnitudeMethods].amp_fact_ml_hb),
			       &(Magnitude[NumMagnitudeMethods].hb_n), &(Magnitude[NumMagnitudeMethods].hb_K),
			       &(Magnitude[NumMagnitudeMethods].hb_Ro), &(Magnitude[NumMagnitudeMethods].hb_Mo));
		sprintf(MsgStr, "LOCMAGNITUDE:  Type: %s  f %e  n %f  K %f  Ro %f  Mo %f",
			mag_type, Magnitude[NumMagnitudeMethods].amp_fact_ml_hb, Magnitude[NumMagnitudeMethods].hb_n,
			Magnitude[NumMagnitudeMethods].hb_K,
			Magnitude[NumMagnitudeMethods].hb_Ro, Magnitude[NumMagnitudeMethods].hb_Mo);
		putmsg(3, MsgStr);

		ierr = 0;
		if (checkRangeDouble("LOCMAG", "f", Magnitude[NumMagnitudeMethods].amp_fact_ml_hb, 1, 0.0, 0, 0.0) != 0)
			ierr = -1;

		if (istat < 4)
			return(-1);

	} else if (strcmp(mag_type, "MD_FMAG") == 0) {

		Magnitude[NumMagnitudeMethods].type = MAG_MD_FMAG;
		istat = sscanf(line1, "%s %lf %lf %lf %lf %lf",
			       mag_type, &(Magnitude[NumMagnitudeMethods].fmag_c1), &(Magnitude[NumMagnitudeMethods].fmag_c2),
			       &(Magnitude[NumMagnitudeMethods].fmag_c3), &(Magnitude[NumMagnitudeMethods].fmag_c4), &(Magnitude[NumMagnitudeMethods].fmag_c5));
		sprintf(MsgStr, "LOCMAGNITUDE:  Type: %s  C1 %lf  C2 %lf  C3 %lf  C4 %lf  C5 %lf",
			mag_type, Magnitude[NumMagnitudeMethods].fmag_c1, Magnitude[NumMagnitudeMethods].fmag_c2, Magnitude[NumMagnitudeMethods].fmag_c3,
			Magnitude[NumMagnitudeMethods].fmag_c4, Magnitude[NumMagnitudeMethods].fmag_c5);
		putmsg(3, MsgStr);

		if (istat != 6)
			return(-1);

	} else {
		Magnitude[NumMagnitudeMethods].type = MAG_UNDEF;
		puterr2("ERROR: unrecognized magnitude calculation type:", mag_type);
	}

	NumMagnitudeMethods++;


	return(0);
}




/** function to read phase statistics params ***/

int GetNLLoc_PhaseStats(char* line1)
{
	int istat;


	istat = sscanf(line1, "%lf %d %lf %lf %lf %lf %lf %lf",
		       &RMS_Max, &NRdgs_Min, &Gap_Max, &P_ResidualMax, &S_ResidualMax, &Ell_Len3_Max, &Hypo_Depth_Min, &Hypo_Depth_Max);

	if (istat < 6)
		Ell_Len3_Max = VERY_LARGE_DOUBLE;
	if (istat < 7)
		Hypo_Depth_Min = -VERY_LARGE_DOUBLE;
	if (istat < 8)
		Hypo_Depth_Max = VERY_LARGE_DOUBLE;

	sprintf(MsgStr,
		"LOCPHSTAT:  RMS_Max: %lf  NRdgs_Min: %d  Gap_Max: %lf  P_ResidualMax: %lf S_ResidualMax: %lf Ell_Len3_Max %lf Hypo_Depth_min %lf Hypo_Depth_max %lf",
		RMS_Max, NRdgs_Min, Gap_Max,
		P_ResidualMax, S_ResidualMax, Ell_Len3_Max, Hypo_Depth_Min, Hypo_Depth_Max);
	putmsg(3, MsgStr);

	if (istat < 5)
		return(-1);

	return(0);
}


/** function to read angles mode params ***/

		int GetNLLoc_Angles(char* line1)
{
	char strAngleMode[MAXLINE];


	sscanf(line1, "%s %d", strAngleMode, &iAngleQualityMin);

	sprintf(MsgStr, "LOCANGLES:  %s  %d", strAngleMode, iAngleQualityMin);
	putmsg(4, MsgStr);

	if (strcmp(strAngleMode, "ANGLES_YES") == 0)
		angleMode = ANGLE_MODE_YES;
	else if (strcmp(strAngleMode, "ANGLES_NO") == 0)
		angleMode = ANGLE_MODE_NO;
	else {
		angleMode = ANGLE_MODE_UNDEF;
		puterr("ERROR: unrecognized angle mode");
		return(-1);
	}

	return(0);

}



/** function to read component description ***/

int GetCompDesc(char* line1)
{
	int istat, ierr;

	if (NumCompDesc >= MAX_NUM_COMP_DESC) {
		sprintf(MsgStr, "%s", line1);
		putmsg(1, MsgStr);
		sprintf(MsgStr,
			"WARNING: maximum number of component descriptions reached, ignoring description.");
		putmsg(1, MsgStr);
		return(-1);
	}

	Component[NumCompDesc].sta_corr_md_fmag = 1.0;	// fmag default

	istat = sscanf(line1, "%s %s %s %lf %lf %lf",
		       Component[NumCompDesc].label, Component[NumCompDesc].inst,
		       Component[NumCompDesc].comp, &(Component[NumCompDesc].amp_fact_ml_hb),
		       &(Component[NumCompDesc].sta_corr_ml_hb),
		       &(Component[NumCompDesc].sta_corr_md_fmag));

	sprintf(MsgStr,
		"LOCCMP:  Label: %s  Inst: %s  Comp: %s  Afact: %lf  StaCorr_ML_HB: %lf  StaCorr_MD_FMAG: %lf",
		Component[NumCompDesc].label, Component[NumCompDesc].inst,
		Component[NumCompDesc].comp, Component[NumCompDesc].amp_fact_ml_hb,
		Component[NumCompDesc].sta_corr_ml_hb,
		Component[NumCompDesc].sta_corr_md_fmag);
	putmsg(3, MsgStr);

	ierr = 0;
	if (checkRangeDouble("LOCCMP", "amp_fact_ml_hb",
	    Component[NumCompDesc].amp_fact_ml_hb, 1, 0.0, 0, 0.0) != 0)
		ierr = -1;

	if (ierr < 0 || istat < 5)
		return(-1);

	NumCompDesc++;

	return(0);
}




/** function to read arrival label alias ***/

int GetLocAlias(char* line1)
{

	if (NumLocAlias >= MAX_NUM_LOC_ALIAS) {
		sprintf(MsgStr, "%s", line1);
		putmsg(1, MsgStr);
		sprintf(MsgStr,
			"WARNING: maximum number of aliases reached, ignoring alias.");
		putmsg(1, MsgStr);
		return(-1);
	}

	sscanf(line1, "%s %s  %d %d %d  %d %d %d",
	       LocAlias[NumLocAlias].name, LocAlias[NumLocAlias].alias,
	       &(LocAlias[NumLocAlias].byr), &(LocAlias[NumLocAlias].bmo),
	       &(LocAlias[NumLocAlias].bday),
	       &(LocAlias[NumLocAlias].eyr), &(LocAlias[NumLocAlias].emo),
	       &(LocAlias[NumLocAlias].eday));

	sprintf(MsgStr,
		"LOCALIAS:  Name: %s  Alias: %s  Valid: %4.4d %2.2d %2.2d -> %4.4d %2.2d %2.2d",
		LocAlias[NumLocAlias].name, LocAlias[NumLocAlias].alias,
		LocAlias[NumLocAlias].byr, LocAlias[NumLocAlias].bmo,
		LocAlias[NumLocAlias].bday,
		LocAlias[NumLocAlias].eyr, LocAlias[NumLocAlias].emo,
		LocAlias[NumLocAlias].eday);
	putmsg(3, MsgStr);

	NumLocAlias++;

	return(0);
}



/** function to read exclude arrival label and phase ***/

int GetLocExclude(char* line1)
{

	if (NumLocExclude >= MAX_NUM_LOC_EXCLUDE) {
		sprintf(MsgStr, "%s", line1);
		putmsg(1, MsgStr);
		sprintf(MsgStr,
			"WARNING: maximum number of excluded phases reached, ignoring exclude.");
		putmsg(1, MsgStr);
		return(-1);
	}

	sscanf(line1, "%s %s",
	       LocExclude[NumLocExclude].label, LocExclude[NumLocExclude].phase);

	if (message_flag >= 3) {
		sprintf(MsgStr, "LOCEXCLUDE:  Name: %s  Phase: %s",
			LocExclude[NumLocExclude].label, LocExclude[NumLocExclude].phase);
		putmsg(3, MsgStr);
	}

	NumLocExclude++;

	return(0);
}




/** function to read station phase time delays ***/

int GetTimeDelays(char* line1)
{

	if (NumTimeDelays >= MAX_NUM_STA_DELAYS) {
		sprintf(MsgStr, "%s", line1);
		putmsg(3, MsgStr);
		sprintf(MsgStr,
			"WARNING: maximum number of station delays reached, ignoring alias.");
		putmsg(2, MsgStr);
		return(-1);
	}

	sscanf(line1, "%s %s %d %lf %lf",
	       TimeDelay[NumTimeDelays].label, TimeDelay[NumTimeDelays].phase,
	       &(TimeDelay[NumTimeDelays].n_residuals),
	       &(TimeDelay[NumTimeDelays].delay),
	       &(TimeDelay[NumTimeDelays].std_dev));

	if (message_flag >= 3) {
		sprintf(MsgStr,
			"LOCDELAY:  Label: %s  Phase: %s  NumResiduals: %d  TimeDelay: %lf  StdDev: %lf",
			TimeDelay[NumTimeDelays].label, TimeDelay[NumTimeDelays].phase,
			TimeDelay[NumTimeDelays].n_residuals,
			TimeDelay[NumTimeDelays].delay,
			TimeDelay[NumTimeDelays].std_dev);
		putmsg(3, MsgStr);
	}

	NumTimeDelays++;

	return(0);
}





/** function to read topo surface (GMT GRD file with x=long, y=lat, z=delay in sec ***/

int GetTopoSurface(char* line1)
{

	int idump_decimation = 0;
	char dump_file[FILENAME_MAX];


	// initialize topo surface fields
	topo_surface = model_surface + (MAX_SURFACES - 1);
	topo_surface_index = MAX_SURFACES - 1;

	sscanf(line1, "%s %d",
	       topo_surface->grd_file, &idump_decimation);

	sprintf(MsgStr, "LOCTOPO_SURFACE:  GMT GRD File: %s  Dump to file decimation: %d",
		topo_surface->grd_file, idump_decimation);
	//putmsg(3, MsgStr);
	putmsg(0, MsgStr);

	if (read_grd(topo_surface, message_flag > 2) < 0) {
		puterr2("ERROR: reading Topo Surface GMT GRD File: ",
			topo_surface->grd_file);
		return(-1);
	}

	if (idump_decimation) {
		strcpy(dump_file, topo_surface->grd_file);
		strcat(dump_file, ".bin");
		dump_grd(topo_surface_index, idump_decimation, 1.0, 1.0, -0.001, dump_file);
		sprintf(MsgStr, "LOCTOPO_SURFACE:  Grid dumped to: %s", dump_file);
		putmsg(0, MsgStr);
	}

	return(0);
}




/** function to read time delay surface (GMT GRD file with x=long, y=lat, z=delay in sec ***/

int GetTimeDelaySurface(char* line1)
{

	sscanf(line1, "%s %lf %s",
	       TimeDelaySurfacePhase[NumTimeDelaySurface],
	       &TimeDelaySurfaceMultiplier[NumTimeDelaySurface],
	       model_surface[NumTimeDelaySurface].grd_file);

	if (message_flag >= 1) {
		sprintf(MsgStr, "LOCDELAY_SURFACE:  Phase: %s  Mult: %f  GMT GRD File: %s",
			TimeDelaySurfacePhase[NumTimeDelaySurface],
			TimeDelaySurfaceMultiplier[NumTimeDelaySurface],
			model_surface[NumTimeDelaySurface].grd_file);
		//putmsg(3, MsgStr);
		putmsg(1, MsgStr);
	}

	if (read_grd(&model_surface[NumTimeDelaySurface], message_flag > 2) < 0) {
		puterr2("ERROR: reading Surface Delay GMT GRD File: ",
			model_surface[NumTimeDelaySurface].grd_file);
		return(-1);
	}

	NumTimeDelaySurface++;

	return(0);
}





/** function to read elevation correction params ***/

int GetElevCorr(char* line1)
{

	int istat;

	istat = sscanf(line1, "%d %lf %lf",
		       &ApplyElevCorrFlag, &ElevCorrVelP, &ElevCorrVelS);

	sprintf(MsgStr, "LOCELEVCORR:  Flag: %d  VelP: %lf  VelS: %lf",
		ApplyElevCorrFlag, ElevCorrVelP, ElevCorrVelS);
	//putmsg(3, MsgStr);
	putmsg(1, MsgStr);

	if (istat != 3)
		return(-1);

	return(0);
}






/** function to open summary output files */

int OpenSummaryFiles(char *path_output, char* typename)
{

	int ngrid;
	char fname[FILENAME_MAX];



	for (ngrid = 0; ngrid < NumLocGrids; ngrid++) {

		if (!LocGridSave[ngrid])
			continue;

		/* Grid Hyp format */

		pSumFileHypNLLoc[ngrid] = NULL;
		sprintf(fname, "%s.sum.%s%d.loc.hyp", path_output, typename, ngrid);
		if ((pSumFileHypNLLoc[ngrid] = fopen(fname, "w")) == NULL) {
			puterr2("ERROR: opening summary output file", fname);
			return(-1);
		} else {
			NumFilesOpen++;
		}


		iWriteHypHeader[ngrid] = 1;

		/* Hypo71 format */
		pSumFileHypo71[ngrid] = NULL;
		if (iSaveHypo71Sum) {
			sprintf(fname, "%s.sum.%s%d.loc.hypo_71", path_output, typename, ngrid);
			if ((pSumFileHypo71[ngrid] = fopen(fname, "w"))
						  == NULL) {
				puterr2(
						"ERROR: opening HYPO71 summary output file",
				fname);
				return(-1);
						  } else {
							  NumFilesOpen++;
						  }
						  fprintf(pSumFileHypo71[ngrid], "%s\n",
								  Hypocenter.comment);
		}


		/* HypoEllipse format */
		pSumFileHypoEll[ngrid] = NULL;
		if (iSaveHypoEllSum) {
			sprintf(fname, "%s.sum.%s%d.loc.hypo_ell", path_output, typename, ngrid);
			if ((pSumFileHypoEll[ngrid] = fopen(fname, "w"))
						  == NULL) {
				puterr2(
						"ERROR: opening HypoEllipse summary output file",
				fname);
				return(-1);
						  } else {
							  NumFilesOpen++;
						  }
						  fprintf(pSumFileHypoEll[ngrid], "%s\n",
								  Hypocenter.comment);
		}


		/* HypoInverse Archive format */
		pSumFileHypoInv[ngrid] = NULL;
		if (iSaveHypoInvSum) {
			sprintf(fname, "%s.sum.%s%d.loc.hypo_inv", path_output, typename, ngrid);
			if ((pSumFileHypoInv[ngrid] = fopen(fname, "w"))
						  == NULL) {
				puterr2(
						"ERROR: opening HypoInverse Archive summary output file",
				fname);
				return(-1);
						  } else {
							  NumFilesOpen++;
						  }
		}

		/* HypoInverse Archive Y2000 format */
		pSumFileHypoInvY2K[ngrid] = NULL;
		if (iSaveHypoInvY2KArc) {
			sprintf(fname, "%s.sum.%s%d.loc.arc", path_output, typename, ngrid);
			if ((pSumFileHypoInvY2K[ngrid] = fopen(fname, "w"))
						  == NULL) {
				puterr2(
						"ERROR: opening HypoInverse Archive Y2000 summary output file",
				fname);
				return(-1);
						  } else {
							  NumFilesOpen++;
						  }
		}

		/* Alberto 3D 4 chr sta SIMULPS format */
		pSumFileAlberto4[ngrid] = NULL;
		if (iSaveAlberto4Sum) {
			sprintf(fname, "%s.sum.%s%d.loc.sim", path_output, typename, ngrid);
			if ((pSumFileAlberto4[ngrid] = fopen(fname, "w"))
						  == NULL) {
				puterr2(
						"ERROR: opening Alberto 3D, 4 chr sta, SIMULPS output file",
				fname);
				return(-1);
						  } else {
							  NumFilesOpen++;
						  }
		}

	}


	return(0);

}



/** function to close summary output files */

int CloseSummaryFiles()
{
	int ngrid;


	for (ngrid = 0; ngrid < NumLocGrids; ngrid++) {

		if (!LocGridSave[ngrid])
			continue;

		/* Grid Hyp format */

		if (pSumFileHypNLLoc[ngrid] != NULL) {
			fclose(pSumFileHypNLLoc[ngrid]);
			pSumFileHypNLLoc[ngrid] = NULL;
			NumFilesOpen--;
		}

		/* Hypo71 format */

		if (pSumFileHypo71[ngrid] != NULL) {
			fclose(pSumFileHypo71[ngrid]);
			NumFilesOpen--;
		}

		/* HypoEll format */

		if (pSumFileHypoEll[ngrid] != NULL) {
			fclose(pSumFileHypoEll[ngrid]);
			NumFilesOpen--;
		}

		/* HypoInv format */

		if (pSumFileHypoInv[ngrid] != NULL) {
			fclose(pSumFileHypoInv[ngrid]);
			NumFilesOpen--;
		}

		/* HypoInv Y2000 format */

		if (pSumFileHypoInvY2K[ngrid] != NULL) {
			fclose(pSumFileHypoInvY2K[ngrid]);
			NumFilesOpen--;
		}

		/* Alberto 3D 4 chr sta SIMULPS format */
		if (pSumFileAlberto4[ngrid] != NULL) {
			fclose(pSumFileAlberto4[ngrid]);
			NumFilesOpen--;
		}


	}

	return(0);

}



/** function to write hypocenter and arrivals to file (Alberto 4 SIMULPS) */

int WriteHypoAlberto4(FILE *fpio, HypoDesc* phypo, ArrivalDesc* parrivals, char* filename)
{

	int ifile = 0, narr;
	char fname[FILENAME_MAX];
	double mag;
	ArrivalDesc* parr;
	int nlat, nlon;


	/* set hypocenter parameters */
	if (phypo->amp_mag != MAGNITUDE_NULL)
		mag = phypo->amp_mag;
	else if (phypo->dur_mag != MAGNITUDE_NULL)
		mag = phypo->dur_mag;
	else
		mag = 0.0;


	/* write hypocenter to file */

	if (fpio == NULL) {
		sprintf(fname, "%s.loc.sim", filename);
		if ((fpio = fopen(fname, "w")) == NULL) {
			puterr("ERROR: opening Alberto 4 hypocenter output file.");
			return(-1);
		} else {
			NumFilesOpen++;
		}
		ifile = 1;
	}


	/* write hypocenter parameters */

//87 1 1  1 2  0.00 35N14.26 120W44.00   5.15   0.00
	nlat = (int) fabs(phypo->dlat);
	nlon = (int) fabs(phypo->dlong);
	fprintf(fpio,
		"%2.2d%2.2d%2.2d %2.2d%2.2d%6.2f %2.2d%c%5.2f %3.3d%c%5.2f %6.2f %6.2f",
		phypo->year % 100, phypo->month, phypo->day,
		phypo->hour, phypo->min, phypo->sec,
		nlat, phypo->dlat > 0.0 ? 'N' : 'S',
		60.0 * (fabs(phypo->dlat) - (double) nlat),
		nlon, phypo->dlong > 0.0 ? 'E' : 'W',
		60.0 * (fabs(phypo->dlong) - (double) nlon),
		phypo->depth, mag
	       );

	// write arrivals

	for (narr = 0; narr < phypo->nreadings; narr++) {
		if (narr % 5 == 0)
			fprintf(fpio, "\n");
		parr = parrivals + narr;
		fprintf(fpio, "%4s%1s%1s%2.2d%7.4f",
			parr->label,
			strcmp(parr->onset,  ARRIVAL_NULL_STR) == 0 ? "i" : parr->onset,
			parr->phase,
			parr->min, parr->sec
		       );
	}

	fprintf(fpio, "\n");

	if (ifile) {
		fclose(fpio);
		NumFilesOpen--;
	}

	return(0);

}



/** function to write hypocenter summary to file (quasi HypoEllipse format) */

int WriteHypoEll(FILE *fpio, HypoDesc* phypo, ArrivalDesc* parrivals,
		 char* filename, int write_header, int write_arrivals)
{

	int ifile = 0, narr;
	char fname[FILENAME_MAX];
	double mag;
	ArrivalDesc* parr;
	double tpobs, resid;


	/* set hypocenter parameters */
	if (phypo->amp_mag != MAGNITUDE_NULL)
		mag = phypo->amp_mag;
	else if (phypo->dur_mag != MAGNITUDE_NULL)
		mag = phypo->dur_mag;
	else
		mag = 0.0;


	/* write hypocenter to file */

	if (fpio == NULL) {
		sprintf(fname, "%s.loc.hypo_ell", filename);
		if ((fpio = fopen(fname, "w")) == NULL) {
			puterr("ERROR: opening hypocenter output file.");
			return(-1);
		} else {
			NumFilesOpen++;
		}
		ifile = 1;
	}


	/* write hypocenter parameters */

	if (write_header) {
		fprintf(fpio,
			"DATE     ORIGIN     LAT         LONG         DEPTH   ");
		fprintf(fpio,
			"MAG  NO  GAP D1     RMS   ");
		fprintf(fpio,
			"AZ1  DIP1 SE1    AZ2  DIP2 SE2    SE3    \n");
		/*"ERH  ERZ Q SQD  ADJ IN NR  AVR  AAR NM AVXM SDXM NF AVFM SDFM I\n");*/
	}
	fprintf(fpio,
		"%4.4d%2.2d%2.2d %2.2d%2.2d %5.2lf %3d %1c %5.2lf %4d %1c %5.2lf %7.3lf ",
		phypo->year, phypo->month, phypo->day,
		phypo->hour, phypo->min, phypo->sec,
		(int) fabs(phypo->dlat), (phypo->dlat >= 0.0 ? 'N' : 'S'),
		(fabs(phypo->dlat) - (int) fabs(phypo->dlat)) * 60.0,
		(int) fabs(phypo->dlong), (phypo->dlong >= 0.0 ? 'E' : 'W'),
		(fabs(phypo->dlong) - (int) fabs(phypo->dlong)) * 60.0,
		phypo->depth);
	fprintf(fpio, "%4.2lf %3d %3d %6.2lf %5.2lf ",
		mag, phypo->nreadings, phypo->gap, phypo->dist, phypo->rms);
	fprintf(fpio, "%4d %4d %6.2lf %4d %4d %6.2lf %6.2lf ",
		(int) (0.5 + phypo->ellipsoid.az1),
		(int) (0.5 + phypo->ellipsoid.dip1),
		phypo->ellipsoid.len1,
		(int) (0.5 + phypo->ellipsoid.az2),
		(int) (0.5 + phypo->ellipsoid.dip2),
		phypo->ellipsoid.len2,
		phypo->ellipsoid.len3);
	fprintf(fpio, "\n");



	if (write_arrivals) {

		fprintf(fpio, "\n");


		fprintf(fpio,
			"  STN  DIST AZM AIN PRMK HRMN P-SEC TPOBS TPCAL DLY/H1 P-RES P-WT AMX PRX CALX K XMAG RMK FMP FMAG\n");
		/*"  STN  DIST AZM AIN PRMK HRMN P-SEC TPOBS TPCAL DLY/H1 P-RES P-WT AMX PRX CALX K XMAG RMK FMP FMAG SRMK S-SEC TSOBS S-RES  S-WT    DT\n"); */

		for (narr = 0; narr < phypo->nreadings; narr++) {
			parr = parrivals + narr;
			tpobs = parr->obs_travel_time > -9.99 ? parr->obs_travel_time : 0.0;
			resid = parr->residual > -99.99 ? parr->residual : -99.99;
			fprintf(fpio,
				"%5s %5.1lf %3d %3d %2s%1s%1d %2.2d%2.2d %5.2lf %5.2lf %5.2lf       %-6.2lf %5.2lf\n",
				parr->label, parr->dist,
				(int) (0.5 + rect2latlonAngle(0, parr->ray_azim)),
				(int) (0.5 + parr->ray_dip),
				parr->phase, parr->first_mot, parr->quality,
				parr->hour, parr->min, parr->sec,
				tpobs, parr->pred_travel_time,
				resid, parr->weight
			       );
		}

	}


	if (ifile) {
		fclose(fpio);
		NumFilesOpen--;
	}

	return(0);

}




/** function to write hypocenter summary to file (HYPO71 format) */

int WriteHypo71(FILE *fpio, HypoDesc* phypo, ArrivalDesc* parrivals,
		char* filename, int write_header, int write_arrivals)
{

	int ifile = 0, narr;
	char fname[FILENAME_MAX];
	ArrivalDesc* parr;
	double tpobs, resid;
	double mag, erh, erz;
	char qualS, qualD, qual;
	int pha_qual;
	double xmag, fmag;

	/* set hypocenter parameters */
	if (phypo->amp_mag != MAGNITUDE_NULL)
		mag = phypo->amp_mag;
	else if (phypo->dur_mag != MAGNITUDE_NULL)
		mag = phypo->dur_mag;
	else
		mag = 0.0;

	/* write hypocenter to file */

	if (fpio == NULL) {
		sprintf(fname, "%s.loc.h71", filename);
		if ((fpio = fopen(fname, "w")) == NULL) {
			puterr("ERROR: opening hypocenter output file.");
			return(-1);
		} else {
			NumFilesOpen++;
		}
		ifile = 1;
	}


	/* write hypocenter parameters */

	if (write_header) {
		fprintf(fpio,
			"  DATE    ORIGIN    LAT      LONG      DEPTH    ");
		fprintf(fpio,
			"MAG NO DM GAP M  RMS  ERH  ERZ Q SQD  ADJ IN NR  AVR  AAR NM AVXM SDXM NF AVFM SDFM I\n");
	}

	fprintf(fpio,
		" %2.2d%2.2d%2.2d %2.2d%2.2d %5.2lf%3d %5.2lf%4d %5.2lf %6.2lf",
		phypo->year % 100, phypo->month, phypo->day,
		phypo->hour, phypo->min, phypo->sec,
		(int) phypo->dlat, /*(phypo->dlat >= 0.0 ? 'N' : 'S'),*/
		(phypo->dlat - (int) phypo->dlat) * 60.0,
		(int) phypo->dlong, /*(phypo->dlong >= 0.0 ? 'E' : 'W'),*/
		(phypo->dlong - (int) phypo->dlong) * 60.0,
		phypo->depth);

	fprintf(fpio, " %6.2lf%3d%3d %3d 0%5.2lf",
		mag, phypo->nreadings, (int) (0.5 + phypo->dist),
		phypo->gap, phypo->rms);

	erh = sqrt(phypo->cov.xx * phypo->cov.xx
			+ phypo->cov.yy * phypo->cov.yy);
	erz = sqrt(phypo->cov.zz * phypo->cov.zz);
	fprintf(fpio, "%5.1lf%5.1lf", erh, erz);

	/* ABCD quality levels (from HYPO71 open file report 75-311, p. 27) */
	if (     phypo->rms < 0.15 && erh <= 1.0 && erz <= 2.0)
		qualS = 'A';
	else if (phypo->rms < 0.30 && erh <= 2.5 && erz <= 5.0)
		qualS = 'B';
	else if (phypo->rms < 0.50 && erh <= 5.0)
		qualS = 'C';
	else
		qualS = 'D';
	if (     phypo->nreadings >= 6 && phypo->gap <= 90
			&& (phypo->dist <= phypo->depth || phypo->dist <= 5.0))
		qualD = 'A';
	else if (phypo->nreadings >= 6 && phypo->gap <= 135
			&& (phypo->dist <= 2.0 * phypo->depth
			|| phypo->dist <= 10.0))
		qualD = 'B';
	else if (phypo->nreadings >= 6 && phypo->gap <= 180
			&& phypo->dist <= 50.0)
		qualD = 'C';
	else
		qualD = 'D';
	/*if (abs(qualS - qualD) == 1)
	qual = qualS < qualD ? qualS : qualD;
	else*/
	qual = 1 + (qualS + qualD) / 2;
	fprintf(fpio, " %1c %1c %1c", qual, qualS, qualD);

	/* dummy values for remaining fields */
	fprintf(fpio,
		" %4.2lf %2d %2d-%4.2lf %4.2lf %2d %4.1lf %4.1lf %2d %4.1lf %4.1lf%2d\n",
		0.0, 0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0);



	if (write_arrivals) {

		fprintf(fpio, "\n");


		fprintf(fpio,
			"  STN  DIST AZM AIN PRMK HRMN P-SEC TPOBS TPCAL DLY/H1 P-RES P-WT AMX PRX CALX K XMAG RMK FMP FMAG SRMK S-SEC TSOBS S-RES  S-WT    DT\n");

		for (narr = 0; narr < phypo->nreadings; narr++) {
			parr = parrivals + narr;
			pha_qual = (parr->quality >= 0 && parr->quality <= 4) ?
					parr->quality : Err2Qual(parr);
			if (pha_qual < 0)
				pha_qual = 4;
			tpobs = parr->obs_travel_time > -9.99 ?
					parr->obs_travel_time : 0.0;
			resid = parr->residual > -99.99 ? parr->residual : -99.99;
			fprintf(fpio,
				/*" %-4s %5.1lf %3d %3d %2s%1s%1d %2.2d%2.2d %5.2lf %5.2lf %5.2lf  0.00 %6.3lf %5.2lf\n", */
				" %-4s %5.1lf %3d %3d %2s%1s%1d %2.2d%2.2d %5.2lf %5.2lf %5.2lf  0.00 %-6.2lf %4.2lf",
				parr->label, parr->dist,
				(int) (0.5 + rect2latlonAngle(0, parr->ray_azim)),
				(int) (0.5 + parr->ray_dip),
				parr->phase, parr->first_mot, pha_qual,
				parr->hour, parr->min, parr->sec,
				tpobs, parr->pred_travel_time,
				resid, parr->weight
			       );

			/* set magnitudes */
			xmag = (parr->amp_mag != MAGNITUDE_NULL) ? parr->amp_mag : 0.0;
			fmag = (parr->dur_mag != MAGNITUDE_NULL) ? parr->dur_mag : 0.0;
			fprintf(fpio,
				" 0.0 0.0 0.00 0 %3.2lf 000 00.0 %3.2lf ??4 00.00 00.00 00.00   0.0      \n",
				xmag, fmag
			       );
		}

	}


	if (ifile) {
		fclose(fpio);
		NumFilesOpen--;
	}

	return(0);

}




/** function to write hypocenter summary to file (HYPOINVERSE archive format) */

int WriteHypoInverseArchive(FILE *fpio, HypoDesc *phypo, ArrivalDesc *parrivals, int narrivals,
			    char *filename, int writeY2000, int write_arrivals, double arrivalWeightMax)
{

	int ifile = 0, narr;
	char fname[FILENAME_MAX];
	ArrivalDesc *parr, *psarr;
	double rms, resid, amplitude;
	double dtemp;

	char first_mot;

	int haveSumHdr = 0;

	/* hypocenter fields */
	char loc_remark[] = "NLL", aux_remark[] = " ", aux_remark_prog[] = " ";
	int num_S_wt = 0, num_P_fmot = 0;
	double amp_mag = 0.0, dur_mag = 0.0;
	double amp_mag_wt = 0.0, dur_mag_wt = 0.0;
	double err_horiz = 0.0, err_vert = 0.0;
	int az_err_prin = 0.0, az_err_inter = 0.0;
	int dip_err_prin = 0.0, dip_err_inter = 0.0;

	/* arrival fields */
	char sta_remark[] = " ";
	int amp_mag_wt_code = 0, dur_mag_wt_code = 0;


	/* if Y2000 and saved HypoInverseArchiveSumHdr, extract hypo fields */
	/* see ftp://ehzftp.wr.usgs.gov/klein/hyp2000/docs/hyp2000-1.0.htm#_Toc7234741 */
	if (writeY2000 && strlen(HypoInverseArchiveSumHdr) > 0) {
		haveSumHdr = 1;
		//printf("HypoInverseArchiveSumHdr:\n|%s|\n", HypoInverseArchiveSumHdr);
		// 81 1 A1 Auxiliary remark from analyst (i.e. Q for quarry).
		strncpy(aux_remark, HypoInverseArchiveSumHdr + 80, 1);
	}

	/* if Y2000, intialize fields */
	if (writeY2000) {
		for (narr = 0; narr < narrivals; narr++) {
			parr = parrivals + narr;
			// num_S_wt - 83 3 I3 Number of S times with weights greater than 0.1.
			if (IsPhaseID(parr->phase, "S") && parr->weight > 0.1)
				num_S_wt++;
			// 94 3 I3 Number of P first motions.
			if (IsPhaseID(parr->phase, "P")
						 && parr->first_mot[0] != ' '
						 && parr->first_mot[0] != ARRIVAL_NULL_CHR)
				num_P_fmot++;
		}
	}


	/* set hypocenter parameters */
//printf("Ma %4.2f  Md %4.2f  MAGNITUDE_NULL %4.2f\n", phypo->amp_mag, phypo->dur_mag, MAGNITUDE_NULL);
	if (fabs(phypo->amp_mag - MAGNITUDE_NULL) > 0.01) {
		amp_mag = phypo->amp_mag;
		// 97 4 F4.1 Total of amplitude mag weights ~number of readings.*
		amp_mag_wt = (double) phypo->num_amp_mag;
	} else if (haveSumHdr) {
		// 37 3 F3.2 Amplitude magnitude.
		sscanf(HypoInverseArchiveSumHdr + 36, "%3lf", &amp_mag);
		amp_mag /= 100.0;
	} else {
		amp_mag = 0.0;
	}
	if (fabs(phypo->dur_mag - MAGNITUDE_NULL) > 0.01) {
		dur_mag = phypo->dur_mag;
		// 101 4 F4.1 Total of duration mag weights ~number of readings. *
		dur_mag_wt = (double) phypo->num_dur_mag;
	} else if (haveSumHdr) {
		// 71 3 F3.2 Coda duration magnitude.
		sscanf(HypoInverseArchiveSumHdr + 70, "%3lf", &dur_mag);
		dur_mag /= 100.0;
	} else {
		dur_mag = 0.0;
	}
//printf("-> amp_mag %4.2f  dur_mag %4.2f\n", amp_mag, dur_mag);



	/* write hypocenter to file */

	if (fpio == NULL) {
		sprintf(fname, "%s.loc.hypo_inv", filename);
		if ((fpio = fopen(fname, "w")) == NULL) {
			puterr("ERROR: opening hypocenter output file.");
			return(-1);
		} else {
			NumFilesOpen++;
		}
		ifile = 1;
	}


	/* write hypocenter parameters */

	// 1 4 I4 Year. *
	if (writeY2000)
		fprintf(fpio, "%4.4d", phypo->year);
	else
		fprintf(fpio, "%2.2d", phypo->year % 100);
	// 5 8 4I2 Month, day, hour and minute.
	// 13 4 F4.2 rigin time seconds.
 	// 17 2 F2.0 Latitude (deg). First character must not be blank.
 	// 19 1 A1 S for south, blank otherwise.
 	// 20 4 F4.2 Latitude (min).
 	// 24 3 F3.0 Longitude (deg).
 	// 27 1 A1 E for east, blank otherwise.
 	// 28 4 F4.2 Longitude (min).
 	// 32 5 F5.2 Depth (km).
	fprintf(fpio,
		"%2.2d%2.2d%2.2d%2.2d%4.0lf%2.2d%1c%4.0lf%3.3d%1c%4.0lf%5.0lf",
		phypo->month, phypo->day,
		phypo->hour, phypo->min, 100.0 * phypo->sec,
		(int) fabs(phypo->dlat), (phypo->dlat >= 0.0 ? ' ' : 'S'),
		(fabs(phypo->dlat) - (int) fabs(phypo->dlat)) * 6000.0,
		(int) fabs(phypo->dlong), (phypo->dlong >= 0.0 ? 'E' : ' '),
		(fabs(phypo->dlong) - (int) fabs(phypo->dlong)) * 6000.0,
		100.0 * phypo->depth);
	// 37 3 F3.2 Amplitude magnitude. *
	// AJL 20080304 - bug fix: added checks for MAGNITUDE_NULL
	if (writeY2000)
		fprintf(fpio, "%3.0lf", fabs(amp_mag - MAGNITUDE_NULL) > 0.01 ? 100.0 * amp_mag : 0.0);
	else
		fprintf(fpio, "%2.0lf", fabs(amp_mag - MAGNITUDE_NULL) > 0.01 ? 10.0 * amp_mag : 0.0);
	// 40 3 I3 Number of P & S times with final weights greater than 0.1.
	// 43 3 I3 Maximum azimuthal gap, degrees.
	// 46 3 F3.0 Distance to nearest station (km).
	// 49 4 F4.2 RMS travel time residual.
	rms = phypo->rms < 99.99 ? phypo->rms : 99.99;
	fprintf(fpio, "%3.3d%3.3d%3.0lf%4.0lf",
		phypo->nreadings, phypo->gap, phypo->dist, 100.0 * rms);
	// 53 3 F3.0 Azimuth of largest principal error (deg E of N).
 	// 56 2 F2.0 Dip of largest principal error (deg).
	// TODO this is smallest princiapl error !!
	az_err_prin = (int) (0.5 + phypo->ellipsoid.az1);
	dip_err_prin = (int) (0.5 + phypo->ellipsoid.dip1);
	if (dip_err_prin < 0) {
		dip_err_prin += 90;
		az_err_prin += 180;
		if (az_err_prin > 360)
			az_err_prin -= 360;
	}
	az_err_inter = (int) (0.5 + phypo->ellipsoid.az2);
	dip_err_inter = (int) (0.5 + phypo->ellipsoid.dip2);
	if (dip_err_inter < 0) {
		dip_err_inter += 90;
		az_err_inter += 180;
		if (az_err_inter > 360)
			az_err_inter -= 360;
	}
 	// 58 4 F4.2 Size of largest principal error (km).
 	// 62 3 F3.0 Azimuth of intermediate principal error.
 	// 65 2 F2.0 Dip of intermediate principal error.
 	// 67 4 F4.2 Size of intermediate principal error (km).
	/*fprintf(fpio, "%3.3d%2.2d%4.0lf%3.3d%2.2d%4.0lf",
	az_err_prin, dip_err_prin,
	100.0 * phypo->ellipsoid.len1,
	az_err_inter, dip_err_inter,
	100.0 * phypo->ellipsoid.len2);
	*/
	// AJL 20060829
	// AJL 20061124
	fprintf(fpio, "000000000000000000");

 	// 71 3 F3.2 Coda duration magnitude. *
	// 74 3 A3 Event location remark (region), derived from location.
 	// 77 4 F4.2 Size of smallest principal error (km).
	// 81 1 A1 Auxiliary remark from analyst (i.e. Q for quarry).
 	// 82 1 A1 Auxiliary remark from program (i.e. “-“ for depth fixed, etc.).
 	// 83 3 I3 Number of S times with weights greater than 0.1.
 	// 86 4 F4.2 Horizontal error (km).
 	// 90 4 F4.2 Vertical error (km).
	if (writeY2000) {
		fprintf(fpio, "%3.0lf", 100.0 * dur_mag);
		// TODO this is largest princiapl error !!
		dtemp = 100.0 * phypo->ellipsoid.len3;
		fprintf(fpio, "%3.3s%4.0lf", loc_remark, dtemp < 9999.0 ? dtemp : 9999.0);
		fprintf(fpio, "%1.1s%1.1s%3.3d%4.0lf%4.0lf",
			aux_remark, aux_remark_prog, num_S_wt, err_horiz, err_vert);
	} else {
		fprintf(fpio, "%2.0lf", 10.0 * dur_mag);
		dtemp = 100.0 * phypo->ellipsoid.len3;
		fprintf(fpio, "%3.3s%4.0lf", loc_remark, dtemp < 9999.0 ? dtemp : 9999.0);
		fprintf(fpio, "%1.1s%1.1s%2.2d%4.0lf%4.0lf",
			aux_remark, aux_remark_prog, num_S_wt, err_horiz, err_vert);
	}


	if (writeY2000) {
		// 94 3 I3 Number of P first motions. *
		// 97 4 F4.1 Total of amplitude mag weights ~number of readings.*
		// 101 4 F4.1 Total of duration mag weights ~number of readings. *
		fprintf(fpio, "%3.3d%4.0lf%4.0lf", num_P_fmot, 10.0 * amp_mag_wt, 10.0 * dur_mag_wt);
		// TODO - not iniitalized from NLL results
		// 105 3 F3.2 Median-absolute-difference of amplitude magnitudes.
		// 108 3 F3.2 Median-absolute-difference of duration magnitudes.
		// 111 3 A3 3-letter code of crust and delay model.
		fprintf(fpio, "  0  0   ");
		// 114 1 A1 Authority” code, i.e. what network furnished the information.  Hypoinverse passes this code through.
		// 115 1 A1 Most common P & S data source code. (See table 1 below).
		// 116 1 A1 Most common duration data source code. (See cols. 68-69)
		// 117 1 A1 Most common amplitude data source code.
		if (haveSumHdr)
			fprintf(fpio, "%4.4s", HypoInverseArchiveSumHdr + 113);
		else
			fprintf(fpio, "   ");
		// TODO - not iniitalized from NLL results
		// 118 1 A1 Primary coda duration magnitude type code
		fprintf(fpio, "D");
		// 119 3 I3 Number of valid P & S readings (assigned weight > 0)
		fprintf(fpio, "%3.3d", phypo->nreadings);
		// TODO - not iniitalized from NLL results
		// 122 1 A1 Primary amplitude magnitude type code
		fprintf(fpio, "X");
		// 123 1    A1  "External" magnitude label or type code. Typically "L" (=ML) This information is not computed by Hypoinverse, but passed along, as computed by UCB.
		// 124 3 F3.2 "External" magnitude.
		// 127 3 F3.1 Total of "external" magnitude weights (~ number of readings).
		// 130 1 A1  Alternate amplitude magnitude label or type code.
		// 131 3 F3.2 Alternate amplitude magnitude.
		// 134 3 F3.1 Total of the alternate amplitude mag weights ~no. of readings.
		// 137 10 I10  Event identification number
		if (haveSumHdr)
			fprintf(fpio, "%24.24s", HypoInverseArchiveSumHdr + 122);
		else
			fprintf(fpio, "%24.24s", "");
		// 147 1 A1 Preferred magnitude label code chosen from those available.
		// 148 3 F3.2 Preferred magnitude, chosen by the Hypoinverse PRE command.
		// 151 4 F4.1 Total of the preferred mag weights (~ number of readings).
		if (phypo->num_amp_mag > 0 && phypo->num_amp_mag >= phypo->num_dur_mag) {
			fprintf(fpio, "X%3.0lf%4.0lf", 100.0 * amp_mag, 10.0 * amp_mag_wt);
		} else if (phypo->num_dur_mag > 0 && phypo->num_dur_mag >= phypo->num_amp_mag) {
			fprintf(fpio, "D%3.0lf%4.0lf", 100.0 * dur_mag, 10.0 * dur_mag_wt);
		} else {
			if (haveSumHdr)
				fprintf(fpio, "%8.8s", HypoInverseArchiveSumHdr + 146);
			else
				fprintf(fpio, "%8.8s", "");
		}
		// 155 1 A1 Alternate coda duration magnitude label or type code.
		// 156 3 F3.2 Alternate coda duration magnitude.
		// 159 4 F4.1 Total of the alternate coda duration magnitude weights. *
		// 163 1 A1 Version” of the information, i.e. the stage of processing.  This can either be passed through, or assigned by Hypoinverse with the LAB command.
		// 164 1 A1  Version of last human review. Hypoinverse passes this through.
		if (haveSumHdr)
			fprintf(fpio, "%10.10s", HypoInverseArchiveSumHdr + 154);
		else
			fprintf(fpio, "%10.10s", "");
		// 164 is the last filled column
	} else {
		fprintf(fpio, "%2.2d", num_P_fmot);
		fprintf(fpio, "                   D   X");
	}

	fprintf(fpio, "\n");



	if (write_arrivals) {

			// following format commens are for Y2000 (station) archive format

// AJL 21JUN2000 bug fix (not all phases are written, looses fm's from phases not used for misfit!
//	for (narr = 0; narr < phypo->nreadings; narr++) {
		for (narr = 0; narr < narrivals; narr++) {
			parr = parrivals + narr;

			/* skip non-P phases */
			if (!IsPhaseID(parr->phase, "P"))
				continue;

			/* check for following S arrival at same station */
			psarr = NULL;
// AJL 21JUN2000
//		if (narr + 1 < phypo->nreadings) {
			if (narr + 1 < narrivals) {
				psarr = parr + 1;
				if (!IsPhaseID(psarr->phase, "S")
								 || strcmp(parr->label, psarr->label) != 0)
					psarr = NULL;
			}

			// 20060619 AJL
			/* no first motion for phases with take-off angle quality < min qual */
			if (angleMode == ANGLE_MODE_YES && parr->ray_qual < iAngleQualityMin)
				first_mot = ' ';
			else if (strpbrk(parr->first_mot, "cCuU+"))
				first_mot = 'U';
			else if (strpbrk(parr->first_mot, "dD-"))
				first_mot = 'D';
			else
				first_mot = ' ';

			if (writeY2000) {
				// 1 5 A5 5-letter station site code, left justified. *
				// 6 2 A2 2-letter seismic network code. *
				// 8 1 1X Blank *
				// 9 1 A1 One letter station component code.
				// 10 3 A3 3-letter station component code. *
				// 13 1 1X Blank *
				fprintf(fpio, "%-5.5s%-2.2s%1.1s%1.1s%-3.3s%1.1s",
					parr->label,
					parr->network != ARRIVAL_NULL_STR ? parr->network : "",
					" ",
					parr->comp != ARRIVAL_NULL_STR ? parr->comp : "",
					parr->inst != ARRIVAL_NULL_STR ? parr->inst : "",
					" ");
				//
				// 14 2 A2 P remark such as "IP".
				// try and reconstruct P remark
				// INGV - "Pn"
				// NCAL - "IP"
				if (strcmp(parr->onset,  ARRIVAL_NULL_STR) == 0) {	// NCAL form
					fprintf(fpio, "%1.1s%1.1s", "", parr->phase);
				} else if (strcmp(parr->onset,  ARRIVAL_NULL_STR) == 0
								       || strpbrk(parr->onset, " iIeE")) {	// NCAL form
									       fprintf(fpio, "%1.1s%1.1s", parr->onset, parr->phase);
								       } else {	// INGV form
									       fprintf(fpio, "%-2.2s", parr->phase);
								       }
				// 16 1 A1 P first motion.
				// 17 1 I1 Assigned P weight code.
								       fprintf(fpio, "%1.1s%1d", parr->first_mot, parr->quality);
			} else {
				fprintf(fpio, "%4.4s%1.1s%1.1s%1c%1d%1.1s",
					parr->label, " ", parr->phase, first_mot,
					parr->quality > 9 ? 9 : parr->quality, " ");
			}
			//
			// 18 4 I4 Year. *
			// 22 8 4I2 Month, day, hour and minute.
			// 30 5 F5.2 Second of P arrival.
			if (writeY2000) {
				fprintf(fpio,
					"%4.4d%2.2d%2.2d%2.2d%2.2d%5.0lf",
					parr->year, parr->month, parr->day,
					parr->hour, parr->min, 100.0 * parr->sec);
			} else {
				fprintf(fpio,
					"%2.2d%2.2d%2.2d%2.2d%2.2d%5.0lf",
					parr->year % 100, parr->month, parr->day,
					parr->hour, parr->min, 100.0 * parr->sec);
			}
			//
			// 35 4 F4.2 P travel time residual.
			// 39 3 F3.2 P weight actually used.
			resid = parr->residual > -9.99 ? parr->residual : 0.0;
			resid = resid < 99.99 ? resid : 99.99;
			fprintf(fpio, "%4.0lf%3.0lf",
				100.0 * resid, 100.0 * parr->weight);
			//
			// 42 5 F5.2 Second of S arrival.
			// 47 2 A2 S remark such as "ES".
			// 49 1 1X Blank
			// 50 1 I1 Assigned S weight code.
			// 51 4 F4.2 S travel time residual.
			//
			// 55 7 F7.2 Amplitude (Normally peak-to-peak). *
			// 62 2 I2 Amp units code. 0=PP mm, 1=0 to peak mm (UCB), 2=digital counts. *
			// 64 3 F3.2 S weight actually used.
			// 67 4 F4.2 P delay time.
			// 71 4 F4.2 S delay time.
			if (psarr != NULL) {
				resid = psarr->residual > -9.99 ? psarr->residual : 0.0;
				fprintf(fpio,
					"%5.0lf%1s%1c %1d%4.0lf",
					100.0 * psarr->sec, " ", psarr->phase[0],
					psarr->quality, 100.0 * resid);
				if (writeY2000) {
					fprintf(fpio, "%7.0lf%2d", 100.0 * parr->amplitude,
							9);	// TODO add true amp units code
				} else {
					amplitude = parr->amplitude != AMPLITUDE_NULL
							? 100.0 * parr->amplitude : 0.0;
					amplitude = amplitude < 99.9 ? amplitude : 99.9;
					fprintf(fpio, "%3.0lf", amplitude);
				}
				fprintf(fpio,
					"%3.0lf%4.0lf%4.0lf",
					100.0 * psarr->weight,
					100.0 * parr->delay, 100.0 * psarr->delay);
			} else {
				if (writeY2000) {
					fprintf(fpio,
							"    0   0   0%7.0lf%2d  0%4.0lf%4.0lf",
							parr->amplitude != AMPLITUDE_NULL
									? 100.0 * parr->amplitude : 0.0,
							9, 100.0 * parr->delay, 0.0);
					// TODO add true amp units code
				} else {
					amplitude = parr->amplitude != AMPLITUDE_NULL
							? 100.0 * parr->amplitude : 0.0;
					amplitude = amplitude < 99.9 ? amplitude : 99.9;
					fprintf(fpio,
							"    0   0   0%3.0lf   %4.0lf%4.0lf", amplitude,
							100.0 * parr->delay, 0.0);
				}
			}
			//
			// 75 4 F4.1 Epicentral distance (km).
			// 79 3 F3.0 Emergence angle at source.
			// 82 1 I1 Amplitude magnitude weight code.
			// 83 1 I1 Duration magnitude weight code.
			// 84 3 F3.2 Period at which the amplitude was measured for this station.
			// 87 1 A1 1-letter station remark.
			// 88 4 F4.0 Coda duration in seconds.
			// 92 3 F3.0 Azimuth to station in degrees E of N.
			fprintf(fpio,
				"%4.0lf%3d%1d%1d%3.0lf%1s%4.0lf%3d",
				GeometryMode == MODE_GLOBAL ? parr->dist * KM2DEG : 10.0 * parr->dist,
				(int) (0.5 + parr->ray_dip),
				amp_mag_wt_code, dur_mag_wt_code,
				parr->period != PERIOD_NULL ? 100.0 * parr->period : 0.0,
				sta_remark, parr->coda_dur,
				(int) (0.5 + rect2latlonAngle(0, parr->ray_azim)));
			//
			// 95 3 F3.2 Duration magnitude for this station. *
			// 98 3 F3.2 Amplitude magnitude for this station. *
			if (writeY2000) {
				fprintf(fpio, "%3.0lf%3.0lf",
					100.0 * parr->dur_mag,
					fabs(parr->amp_mag - MAGNITUDE_NULL) > 0.01 ?
							100.0 * parr->amp_mag : 0.0);
			} else {
				fprintf(fpio, "%2.0lf%2.0lf",
					fabs(parr->dur_mag - MAGNITUDE_NULL) > 0.01 ?
							10.0 * parr->dur_mag : 0.0,
					fabs(parr->amp_mag - MAGNITUDE_NULL) > 0.01 ?
							10.0 * parr->amp_mag : 0.0);
			}
			//
			// 101 4 F4.3 Importance of P arrival.
			// 105 4 F4.3 Importance of S arrival.
			fprintf(fpio,
				"%4.0lf%4.0lf",
				1000.0 * parr->weight / arrivalWeightMax,
				psarr != NULL ? 1000.0 * psarr->weight / arrivalWeightMax : 0.0);
			//
			// 109 1 A1 Data source code.
			// 110 1 A1 Label code for duration magnitude from FC1 or FC2 command.
			// 111 1 A1 Label code for amplitude magnitude from XC1 or XC2 command.
			// 112 2 A2 2-letter station location code (component extension).
			// 113 is the last filled column.
			fprintf(fpio, "     ");	// TODO - add data source?

			fprintf(fpio, "\n");
		}

	}


	fprintf(fpio, "\n");
	if (ifile) {
		fclose(fpio);
		NumFilesOpen--;
	}

	return(0);

}



/** function to determine azimuth gap -
			largest azimuth separation between
			stations as seen from epicenter (deg) */

double CalcAzimuthGap(ArrivalDesc *arrival, int num_arrivals)
{

	int narr, naz;
	double az_last, az, gap, gap_max = -1.0;
	double azimuth[MAX_NUM_ARRIVALS];

	/* load azimuths to working array */
	naz = 0;
	for (narr = 0; narr < num_arrivals; narr++) {
		if ((arrival + narr)->weight > 0.001)
			azimuth[naz++] = (arrival + narr)->azim;
	}

	/* sort */
	SortDoubles(azimuth, naz);

	/* find largest gap */
	az_last = azimuth[0];
	for (narr = 1; narr < naz; narr++) {
		az = azimuth[narr];
		gap = az - az_last;
		if (gap > gap_max)
			gap_max = gap;
		az_last = az;
	}
	az = azimuth[0] + 360.0;
	gap = az - az_last;
	if (gap > gap_max)
		gap_max = gap;

	return(gap_max);

}




/** function to estimate Vp/Vs ration */

/*  follows chapter 5 of Lahr, J.C., 1989. HYPOELLIPSE/Version 2.0: A computer program for
determining local earthquake hypocentral parameters, magnitude and first motion pattern, U.S.
Geological Survey Open-File Report 89-116, 92p.
*/

double CalculateVpVsEstimate(HypoDesc* phypo, ArrivalDesc* arrival, int narrivals)
{

	int narr, npair;
	int k;
	double p_time[MAX_NUM_ARRIVALS], p_time_wt;
	double p_time_cent, p_error[MAX_NUM_ARRIVALS];
	double s_time[MAX_NUM_ARRIVALS], s_time_wt;
	double s_time_cent, s_error[MAX_NUM_ARRIVALS];
	double weight[MAX_NUM_ARRIVALS], weight_sum;
	double B, Btest, Bmin, dB, Tmin, T, temp;
	double tsp, tsp_min_max_diff;
	double tsp_min = VERY_LARGE_DOUBLE;
	double tsp_max = -VERY_LARGE_DOUBLE;

//printf("CALCULATING VpVs\n");

	/* load P S pairs to working arrays (assumes arrivals sorted by distance) */

	npair = 0;
	for (narr = 1; narr < narrivals; narr++) {
		if (strcmp(arrival[narr - 1].label, arrival[narr].label) == 0
				  && IsPhaseID(arrival[narr - 1].phase, "P")
				  && IsPhaseID(arrival[narr].phase, "S")) {
// DELAY_CORR
			// removing delay so add (Tobs = Tcorr + (O-C))
			p_time[npair] = arrival[narr - 1].obs_time + arrival[narr - 1].delay;
			p_error[npair] = arrival[narr - 1].error;
// DELAY_CORR
			// removing delay so add (Tobs = Tcorr + (O-C))
			s_time[npair] = arrival[narr].obs_time + arrival[narr].delay;
			s_error[npair] = arrival[narr].error;
//printf("PAIR %s %s (%lf +/- %lf) + %s %s (%lf +/- %lf)\n", arrival[narr - 1].label, arrival[narr - 1].phase, p_time[npair], p_error[npair], arrival[narr].label, arrival[narr].phase, s_time[npair], s_error[npair]);

			tsp = s_time[npair] - p_time[npair];
			tsp_min = tsp < tsp_min ? tsp : tsp_min;
			tsp_max = tsp > tsp_max ? tsp : tsp_max;

			npair++;
				  }
	}

	tsp_min_max_diff = tsp_max - tsp_min;
	phypo->tsp_min_max_diff = tsp_min_max_diff;

	/* not enough pairs found */
	if (npair < 2) {
		phypo->VpVs = -1.0;
		phypo->nVpVs = npair;
		return(-1.0);
	}


	/* search for optimal VpVs */

	B = Bmin = 2.0;
	Tmin = VERY_LARGE_DOUBLE;
	for (dB = 0.5; dB > 0.00001; dB *= 0.4) {

		for (k = -3; k < 4; k++) {

			Btest = B + (double) k * dB;

			// form weights
			p_time_wt = 0.0;
			s_time_wt = 0.0;
			weight_sum = 0.0;
			for (narr = 1; narr < npair; narr++) {
				weight[narr] = 1.0 /
						(s_error[narr] * s_error[narr]
						+ Btest * p_error[narr] * p_error[narr]);
				p_time_wt += weight[narr] * p_time[narr];
				s_time_wt += weight[narr] * s_time[narr];
				weight_sum += weight[narr];
			}

			// form centered times and accumulate T
			T = 0.0;
			for (narr = 1; narr < npair; narr++) {
				p_time_cent = p_time[narr] - p_time_wt / weight_sum;
				s_time_cent = s_time[narr] - s_time_wt / weight_sum;
				temp = s_time_cent - Btest * p_time_cent;
				T += weight[narr] * temp * temp;
			}

			if (T < Tmin) {
				Tmin = T;
				Bmin = Btest;
//printf("  NEW MIN VpVs = %lf dB = %lf T = %le  k = %d\n", Btest, dB, T, k);
			}

		}

		B = Bmin;
	}


//printf("  OPTIMAL VpVs = %.3lf last dB = %lf npair = %d\n", B, dB / 0.4, npair);

	phypo->VpVs = B;
	phypo->nVpVs = npair;

	return(B);

}




/** function to calculate magintude */

int CalculateMagnitude(HypoDesc* phypo, ArrivalDesc* parrivals,
		       int narrivals, CompDesc* pcomp, int nCompDesc, MagDesc* pmagnitude)
{

	int nmag , narr, nComp;
	double amp_mag_sum, dur_mag_sum;
	double amp_fact_ml_hb, sta_corr;
	ArrivalDesc* parr;



	/* calculate magnitude for specified calculation type */

	if (pmagnitude == MAG_UNDEF) {
		/* no magnitude calculation type given */

		return(0);

	}
	else if (pmagnitude->type == MAG_ML_HB) {
		/* ML - Hutton & Boore, BSSA, v77, n6, Dec 1987 */

		/* write message */
		if (message_flag >= 3) {
			sprintf(MsgStr, "\nComponent results for: ML - Hutton & Boore, BSSA, v77, n6, Dec 1987:");
			putmsg(3, MsgStr);
		}

		nmag = 0;
		amp_mag_sum = 0.0;
		for (narr = 0; narr < narrivals; narr++) {

			/* check for valid amplitude */
			if ((parr = parrivals + narr)->amplitude > 0.0 && parr->amplitude != AMPLITUDE_NULL) {

				nComp = findStaInstComp(parr, pcomp, nCompDesc);
				if (nComp >= 0) {
					/* sta/inst/comp found */
					amp_fact_ml_hb = (pcomp + nComp)->amp_fact_ml_hb;
					sta_corr = (pcomp + nComp)->sta_corr_ml_hb;
				} else {
					amp_fact_ml_hb = 1.0;
					sta_corr = 0.0;
				}

				/* calc ML */
				parr->amp_mag = Calc_ML_HuttonBoore(
						parr->amplitude * pmagnitude->amp_fact_ml_hb * amp_fact_ml_hb,
				parr->dist, phypo->depth, sta_corr,
				pmagnitude->hb_n, pmagnitude->hb_K,
				pmagnitude->hb_Ro, pmagnitude->hb_Mo);

				/* write message */
				if (message_flag >= 3) {
					sprintf(MsgStr, "%s %s %s amp %.2e f %.2e f_sta %.2e dist %.2f depth %.2f sta_corr %.4f hb_n %.2f hb_K %.5f mag %.2f",
							parr->label, parr->inst, parr->comp, parr->amplitude, pmagnitude->amp_fact_ml_hb, amp_fact_ml_hb, parr->dist,
							phypo->depth, sta_corr, pmagnitude->hb_n, pmagnitude->hb_K, parr->amp_mag);
					putmsg(3, MsgStr);
				}


				/* update event magnitude */
				amp_mag_sum += parr->amp_mag;
				nmag++;
			}
		}
		if (nmag > 0) {
			phypo->amp_mag = amp_mag_sum / (double) nmag;
			phypo->num_amp_mag = nmag;
		}
	}
	else if (pmagnitude->type == MAG_MD_FMAG) {
		/* coda duration (FMAG) - HYPOELLIPSE users manual chap 4;
		Lee et al., 1972; Lahr et al., 1975; Bakun and Lindh, 1977 */

		nmag = 0;
		dur_mag_sum = 0.0;
		for (narr = 0; narr < narrivals; narr++) {

			/* check for valid amplitude */
			if ((parr = parrivals + narr)->coda_dur > 0.0 && parr->coda_dur != CODA_DUR_NULL) {

				nComp = findStaInstComp(parr, pcomp, nCompDesc);
				if (nComp >= 0) {
					/* sta/inst/comp found */
					sta_corr = (pcomp + nComp)->sta_corr_md_fmag;
				} else {
					sta_corr = 1.0;
				}

				/* calc ML */
				parr->dur_mag = Calc_MD_FMAG(
						parr->coda_dur, parr->dist, phypo->depth, sta_corr,
				pmagnitude->fmag_c1, pmagnitude->fmag_c2, pmagnitude->fmag_c3,
				pmagnitude->fmag_c4, pmagnitude->fmag_c5);

/*printf("%s %s %s coda_dur %lf dist %lf depth %lf sta_corr %lf c1 %lf c2 %lf c3 %lf c4 %lf c5 %lf mag %lf\n",
				parr->label, parr->inst, parr->comp, parr->coda_dur, parr->dist,
				phypo->depth, sta_corr, pmagnitude->fmag_c1, pmagnitude->fmag_c2, pmagnitude->fmag_c3,
				pmagnitude->fmag_c4, pmagnitude->fmag_c5, parr->dur_mag);
*/

				/* update event magnitude */
				dur_mag_sum += parr->dur_mag;
				nmag++;
			}
		}
		if (nmag > 0) {
			phypo->dur_mag = dur_mag_sum / (double) nmag;
			phypo->num_dur_mag = nmag;
		}
	}

	return(0);
}


/** function to find component parameters */

int findStaInstComp(ArrivalDesc* parr, CompDesc* pcomp, int nCompDesc) {

	int nComp;
	char *pchr, test_label[ARRIVAL_LABEL_LEN];

	strcpy(test_label, parr->time_grid_label);

	for (nComp = 0; nComp < nCompDesc; nComp++) {
		strcpy(test_label, parr->time_grid_label);
		if ((pchr = strrchr(test_label, '_')) != NULL)
			*pchr = '\0';
//printf("comp %s  arr_test %s %s %s\n", (pcomp + nComp)->label, parr->label, parr->time_grid_label, test_label);
		if ((pcomp + nComp)->label[0] != ARRIVAL_NULL_CHR &&
				   (pcomp + nComp)->label[0] != '*' &&
				   strcmp((pcomp + nComp)->label,
					   test_label) != 0)
			continue;
		if ((pcomp + nComp)->inst[0] != ARRIVAL_NULL_CHR &&
				   (pcomp + nComp)->inst[0] != '*' &&
				   strcmp((pcomp + nComp)->inst,
					   parr->inst) != 0)
			continue;
		if ((pcomp + nComp)->comp[0] != ARRIVAL_NULL_CHR &&
				   (pcomp + nComp)->comp[0] != '*' &&
				   strcmp((pcomp + nComp)->comp,
					   parr->comp) != 0)
			continue;
		// found
		return(nComp);
	}

	// not found
	return(-1);
}


/** function to calculate local magnitude ML */
/* ML - Hutton & Boore, BSSA, v77, n6, Dec 1987 */

double Calc_ML_HuttonBoore(double amplitude, double dist, double depth, double sta_corr,
			   double hb_n, double hb_K, double hb_Ro, double hb_Mo)
{

	double hyp_dist, magnitude;

	hyp_dist = sqrt(dist * dist + depth * depth);

	if (hyp_dist < SMALL_DOUBLE)
		return(MAGNITUDE_NULL);

	magnitude = log10(amplitude)
			+ hb_n * log10(hyp_dist / hb_Ro) + hb_K * (hyp_dist - hb_Ro)
			+ hb_Mo + sta_corr;

	return(magnitude);

}



/** function to calculate coda duration magnitude MD */
/* coda duration (FMAG) - HYPOELLIPSE users manual chap 4;
	Lee et al., 1972; Lahr et al., 1975; Bakun and Lindh, 1977 */

double Calc_MD_FMAG(double coda_dur, double dist, double depth, double sta_corr,
		    double fmag_c1, double fmag_c2, double fmag_c3, double fmag_c4, double fmag_c5)
{

	double hyp_dist, magnitude;

	hyp_dist = sqrt(dist * dist + depth * depth);

	if (coda_dur * sta_corr < SMALL_DOUBLE)
		return(MAGNITUDE_NULL);

	magnitude = fmag_c1
			+ fmag_c2 * log10(coda_dur * sta_corr)
			+ fmag_c3 * dist
			+ fmag_c4 * depth
			+ fmag_c5 * pow(log10(coda_dur * sta_corr), 2);

	return(magnitude);

}






/*------------------------------------------------------------/ */
/** hashtable routines for accumulating station statistics */

/* from Kernigham and Ritchie, C prog lang, 2nd ed, 1988, sec 6.6 */


/** funtion to form ordered hash value from firt char of label */

unsigned hash(char* label, char* phase)
{
	unsigned hashval;

	if (isdigit(label[0]))
		hashval = label[0] - '0';
	else if (isalpha(label[0]))
		hashval = 10 + toupper(label[0]) - 'A';
	else
		hashval = 36 + label[0] % 10;

	hashval = hashval % HASHSIZE;

	return hashval;
}


/** function to lookup labelphase in hashtable */

StaStatNode *lookup(int ntable, char* label, char* phase)
{
	StaStatNode *np;

	for (np = hashtab[ntable][hash(label, phase)]; np != NULL;
		    np = np->next)
		if (strcmp(label, np->label) == 0
				    && strcmp(phase, np->phase) == 0)
			return(np);	/* found */

	return(NULL);		/* not found */

}


/** function to install or add (labelphase, residual, weight) in hashtable */

StaStatNode *InstallStaStatInTable(int ntable, char* label, char* phase, int flag_ignore,
				   double residual, double weight,
				   double pdf_residual_sum, double pdf_weight_sum, double delay)
{
	int icomp;
	StaStatNode *np, *npcheck, *nplast;
	unsigned hashval;

	if ((np = lookup(ntable, label, phase)) == NULL)
	{
		/* not found, create new StaStatNode */
		if ((np = (StaStatNode *) malloc(sizeof(StaStatNode))) == NULL)
			return(NULL);
		strcpy(np->label, label);
		strcpy(np->phase, phase);
		np->flag_ignore = flag_ignore;
		np->residual_min = residual;
		np->residual_max = residual;
		np->residual_sum = residual * weight;
		np->residual_square_sum = residual * residual * weight;
		np->weight_sum = weight;
		np->num_residuals = 1;
		if (pdf_weight_sum > VERY_SMALL_DOUBLE) {
			np->pdf_residual_sum =
					pdf_residual_sum / pdf_weight_sum;
			np->pdf_residual_square_sum =
					pdf_residual_sum * pdf_residual_sum / (pdf_weight_sum * pdf_weight_sum);
			np->num_pdf_residuals = 1;
		} else {
			np->num_pdf_residuals = 0;
		}
		np->delay = delay;

		/* put in table in alphabetical order */
		hashval = hash(label, phase);
		nplast = NULL;
		npcheck = hashtab[ntable][hashval];
		while (npcheck != NULL &&
				     (icomp = strcmp(npcheck->label, label)) <= 0) {
			if (icomp == 0 && strcmp(npcheck->phase, phase) >= 0)
				break;
			nplast = npcheck;
			npcheck = npcheck->next;
				     }
				     np->next = npcheck;
				     if (nplast != NULL)
					     nplast->next = np;
				     else
					     hashtab[ntable][hashval] = np;
	}
	else
	{
		/* already there */
		if (residual < np->residual_min)
			np->residual_min = residual;
		if (residual > np->residual_max)
			np->residual_max = residual;
		np->residual_sum += residual * weight;
		np->residual_square_sum += residual * residual * weight;
		np->weight_sum += weight;
		np->num_residuals++;
		if (pdf_weight_sum > VERY_SMALL_DOUBLE) {
			np->pdf_residual_sum +=
					pdf_residual_sum / pdf_weight_sum;
			np->pdf_residual_square_sum +=
					pdf_residual_sum * pdf_residual_sum / (pdf_weight_sum * pdf_weight_sum);
			np->num_pdf_residuals++;
		}
	}

	return(np);

}


/** function to output hashtable values */

		int WriteStaStatTable(int ntable, FILE *fpio,
				      double rms_max, int nRdgs_min, double gap_max,
				      double p_residual_max, double s_residual_max,
				      double ell_len3_max, double hypo_depth_min, double hypo_depth_max, int imode)
{
	int nnodes;
	unsigned hashval;
	char frmt1[MAXLINE], frmt2[MAXLINE];
	double res_temp, res_std_temp;
	StaStatNode *np;

	sprintf(frmt1, "LOCDELAY  %%-%ds %%-%ds %%-8d %%-12lf %%-12lf\n",
		ARRIVAL_LABEL_LEN, ARRIVAL_LABEL_LEN);
	sprintf(frmt2, "LOCDELAY  %%-%ds %%-%ds %%-8d %%-12lf %%-12lf %%-12lf %%-12lf %%d\n",
		ARRIVAL_LABEL_LEN, ARRIVAL_LABEL_LEN);

	if (imode == WRITE_RESIDUALS) {
		fprintf(fpio,
			"\n#Average Phase Residuals (CalcResidual)  RMS_Max: %lf  NRdgs_Min: %d  Gap_Max: %lf  P_Res_Max: %lf  S_Res_Max: %lf  Ell_Len3_Max: %lf  Hypo_Depth_Min: %lf  Hypo_Depth_Max: %lf\n",
			rms_max, nRdgs_min, gap_max,
			p_residual_max, s_residual_max, ell_len3_max, hypo_depth_min, hypo_depth_max);
		fprintf(fpio,
			"#         ID      Phase   Nres      AveRes       StdDev       ResMin       ResMax     ignored\n");
	} else if (imode == WRITE_RES_DELAYS) {
		fprintf(fpio,
			"\n#Total Phase Corrections (CalcResidual + InputDelay)  RMS_Max: %lf  NRdgs_Min: %d  Gap_Max: %lf  P_Res_Max: %lf  S_Res_Max: %lf  Ell_Len3_Max: %lf  Hypo_Depth_Min: %lf  Hypo_Depth_Max: %lf\n",
			rms_max, nRdgs_min, gap_max,
			p_residual_max, s_residual_max, ell_len3_max, hypo_depth_min, hypo_depth_max);
		fprintf(fpio,
			"#         ID      Phase   Nres      TotCorr      StdDev\n");
	} else if (imode == WRITE_PDF_RESIDUALS) {
		fprintf(fpio,
			"\n#Average Phase Residuals PDF (CalcPDFResidual)  RMS_Max: %lf  NRdgs_Min: %d  Gap_Max: %lf  P_Res_Max: %lf  S_Res_Max: %lf  Ell_Len3_Max: %lf  Hypo_Depth_Min: %lf  Hypo_Depth_Max: %lf\n",
			rms_max, nRdgs_min, gap_max,
			p_residual_max, s_residual_max, ell_len3_max, hypo_depth_min, hypo_depth_max);
		fprintf(fpio,
			"#         ID      Phase   Nres      AveRes       StdDev       ResMin       ResMax     ignored\n");
	} else if (imode == WRITE_PDF_DELAYS) {
		fprintf(fpio,
			"\n#Total Phase Corrections PDF (CalcPDFResidual + InputDelay)  RMS_Max: %lf  NRdgs_Min: %d  Gap_Max: %lf  P_Res_Max: %lf  S_Res_Max: %lf  Ell_Len3_Max: %lf  Hypo_Depth_Min: %lf  Hypo_Depth_Max: %lf\n",
			rms_max, nRdgs_min, gap_max,
			p_residual_max, s_residual_max, ell_len3_max, hypo_depth_min, hypo_depth_max);
		fprintf(fpio,
			"#         ID      Phase   Nres      TotCorr      StdDev\n");
	}

	nnodes = 0;
	for (hashval = 0; hashval < HASHSIZE; hashval++) {
		for (np = hashtab[ntable][hashval]; np != NULL; np = np->next) {
			if (imode == WRITE_RESIDUALS || imode == WRITE_RES_DELAYS) {
				res_temp = np->residual_sum / np->weight_sum;
				res_std_temp = np->residual_square_sum / np->weight_sum - res_temp * res_temp;
				if (np->num_residuals > 1)
					res_std_temp = sqrt(np->residual_square_sum / np->weight_sum
							- res_temp * res_temp);
				else
					res_std_temp = -1.0;
				if (imode == WRITE_RESIDUALS) {
					fprintf(fpio, frmt2, np->label, np->phase,
							np->num_residuals, res_temp, res_std_temp,
							np->residual_min, np->residual_max, np->flag_ignore);
				} else if (imode == WRITE_RES_DELAYS) {
					fprintf(fpio, frmt1, np->label, np->phase,
							np->num_residuals, res_temp + np->delay, res_std_temp);
				}
//printf("LOCDELAY  %s %s %d %f = %f + %f/%f\n", np->label, np->phase, np->num_residuals, res_temp,
//np->delay, np->residual_sum, np->weight_sum);
			} else if (imode == WRITE_PDF_RESIDUALS || imode == WRITE_PDF_DELAYS) {
				if (np->num_pdf_residuals > 0) {
					res_temp = np->pdf_residual_sum / (double) np->num_pdf_residuals;
				} else {
					res_temp = 0.0;
				}
				if (np->num_pdf_residuals > 1)
					res_std_temp = sqrt(np->pdf_residual_square_sum
						/ (double) (np->num_pdf_residuals - 1)
							- res_temp * res_temp);
				else
					res_std_temp = -1.0;
				if (imode == WRITE_PDF_RESIDUALS) {
					fprintf(fpio, frmt2, np->label, np->phase,
							np->num_pdf_residuals, res_temp, res_std_temp,
							np->residual_min, np->residual_max, np->flag_ignore);
				} else if (imode == WRITE_PDF_DELAYS) {
					fprintf(fpio, frmt1, np->label, np->phase,
							np->num_pdf_residuals, res_temp + np->delay, res_std_temp);
				}
			}
			nnodes++;
		}
	}


	return(nnodes);

}


/** function to update hashtable */

void UpdateStaStat(int ntable, ArrivalDesc *arrival, int num_arrivals,
		   double p_residual_max, double s_residual_max)
{
	int narr;
	StaStatNode *np;
	double weight = 1.0;

	for (narr = 0; narr < num_arrivals; narr++)
		if ((IsPhaseID((arrival + narr)->phase, "P") &&
				   fabs((arrival + narr)->residual)
				   <= p_residual_max)
				   || (IsPhaseID((arrival + narr)->phase, "S") &&
				   fabs((arrival + narr)->residual)
				   <= s_residual_max))
			if ((np = InstallStaStatInTable(ntable,
			     (arrival + narr)->label,
			     (arrival + narr)->phase,
			     (arrival + narr)->flag_ignore,
			     (arrival + narr)->residual,
			     weight,
			     (arrival + narr)->pdf_residual_sum,
			     (arrival + narr)->pdf_weight_sum,
			     (arrival + narr)->delay)) == NULL)
				puterr("ERROR: cannot put arrival statistics in table");

}


/** end of hashtable routines */
/*------------------------------------------------------------/ */






		/** function to add arrival station to station list */

int addToStationList(SourceDesc *stations, int numStations, ArrivalDesc *arrival, int nArrivals) {

	int i, n, nAdded = 0;;


	// set default ignored flag, weight and nullname
	for (n = 0; n < MAX_NUM_ARRIVALS; n++) {
		(stations + n)->ignored = 1;
		(stations + n)->station_weight = 1.0;
		//strcpy((stations + n)->label, "");
	}


	for (i = 0; i < nArrivals; i++) {

		if (numStations >= MAX_NUM_ARRIVALS)
			return(0);

		n = 0;
		for ( ; n < numStations; n++) {
			if (strcmp((stations + n)->label, (arrival + i)->label) == 0) {
				break;	// already in list
			}
		}

		if (n == numStations) {
			*(stations + n) = (arrival + i)->station;
			strcpy((stations + n)->label, (arrival + i)->label);
			nAdded++;
			numStations++;
			if (message_flag >= 4) {
				sprintf(MsgStr, "Added to station list: %s (%lf,%lf,%lf)",
					(stations + n)->label, (stations + n)->x, (stations + n)->y,
					(stations + n)->z);
				putmsg(4, MsgStr);
			}
		}

		if (!((arrival + i)->flag_ignore))
			(stations + n)->ignored = 0;

	}

	return(numStations);


}



/** function to write station list to file */

int WriteStationList(FILE* fpio, SourceDesc *stations, int numStations) {

	int n;

	for (n = 0; n < numStations; n++) {
		fprintf(fpio, "%s %lf %lf %lf\n",
			(stations + n)->label, (stations + n)->x, (stations + n)->y, (stations + n)->z);
	}

	return(0);

}



/** function to set station distribution weights */

int setStationDistributionWeights(SourceDesc *stations, int numStations, ArrivalDesc *arrival, int nArrivals) {

	int i, nsta, n, m, nError = 0;
	double x, y, dist, station_weight, weight;
	double dist_ave, cutoff2;
	double station_weight_sum;


	// get cutoff distance
	if (stationDistributionWeightCutoff > 0.0) {
		cutoff2 = stationDistributionWeightCutoff * stationDistributionWeightCutoff;
	} else {
	// use average distance
		dist_ave = calcAveInterStationDistance(stations, numStations);
		if (message_flag >= 2) {
			sprintf(MsgStr, "Station Dist Weight:  Ave Station Distance: %lf", dist_ave);
			putmsg(2, MsgStr);
		}
		if (dist_ave <= 0.0)
			return(-1);
		cutoff2 = dist_ave * dist_ave;
	}



	// calculate station weights

	station_weight_sum = 0.0;
	nsta= 0;
	for (n = 0; n < numStations; n++) {
		station_weight = 0.0;
		x = (stations + n)->x;
		y = (stations + n)->y;
		if (x == 0.0 && y == 0.0)	// station location not known
			continue;
		for (m = 0; m < numStations; m++) {
			if ((stations + m)->ignored)
				continue;
			dist = GetEpiDist((stations + m), x, y);
			weight = exp(-(dist * dist) / cutoff2);	// 0.0 for farthest -> 1.0 for same postion
			station_weight += weight;	// count of number close stations
		}
		nsta++;
		station_weight = 1.0 / station_weight;	// weight inversely prop to number of close stations
		(stations + n)->station_weight = station_weight;
//printf("Station Weight: %s %lf (%lf,%lf,%lf)\n", (stations + n)->label, (stations + n)->station_weight, (stations + n)->x, (stations + n)->y, (stations + n)->z);
		station_weight_sum += station_weight;
	}
	if (nsta > 0) {
		// normalize
		station_weight_sum /= (double) nsta;
		for (n = 0; n < numStations; n++) {
			(stations + n)->station_weight = (stations + n)->station_weight / station_weight_sum;
			if (message_flag >= 3) {
				sprintf(MsgStr, "Station Dist Weight: %s %lf (%lf,%lf,%lf)", (stations + n)->label, (stations + n)->station_weight, (stations + n)->x, (stations + n)->y, (stations + n)->z);
				putmsg(2, MsgStr);
			}
		}
	}


	// set arrival weight

	for (i = 0; i < nArrivals; i++) {

		// find station for this arrival in station list
		for (n = 0; n < numStations; n++) {
			if (strcmp((stations + n)->label, (arrival + i)->label) == 0) {
				(arrival + i)->station_weight = (stations + n)->station_weight;
//printf("Arrival Sta Dist Weight: %s %lf (%lf,%lf,%lf)\n", (arrival + i)->label, (arrival + i)->station_weight, (stations + n)->x, (stations + n)->y, (stations + n)->z);
				break;
			}
		}

		// not found (should not occur)
		if (n == numStations) {
			sprintf(MsgStr, "ERROR: setting Station Distance Weights: station not found in station list: %s",
				(arrival + i)->label);
			putmsg(1, MsgStr);
			nError++;
			continue;
		}


	}

	return(nError);


}





/** function to get travel times for all observed arrivals */

int getTravelTimes(ArrivalDesc *arrival, int num_arr_loc, double xval, double yval, double zval)
{
	int nReject;
	int narr, n_compan;
	FILE* fp_grid;
	double yval_grid = 0.0;
	GridDesc* ptgrid;


	/* loop over observed arrivals */

	nReject = 0;
	for (narr = 0; narr < num_arr_loc; narr++) {
		/* check for companion */
		if ((n_compan = arrival[narr].n_companion) >= 0) {
			if ((arrival[narr].pred_travel_time =
						  arrival[n_compan].pred_travel_time) < 0.0)
				nReject++;
			arrival[narr].pred_travel_time *= arrival[narr].tfact;
			/* else check grid type */
		} else {
			if (arrival[narr].gdesc.type == GRID_TIME) {
				/* 3D grid */
				if (arrival[narr].gdesc.buffer == NULL) {
					/* read time grid from disk */
					fp_grid = arrival[narr].fpgrid;
				} else {
					/* read time grid from memory buffer */
					fp_grid = NULL;
				}
				if ((arrival[narr].pred_travel_time = (double) ReadAbsInterpGrid3d(fp_grid, &(arrival[narr].gdesc),
				     xval, yval, zval)) < 0.0)
					nReject++;
			} else {
				/* 2D grid (1D model) */
				yval_grid = GetEpiDist(&(arrival[narr].station), xval, yval);
				if (GeometryMode == MODE_GLOBAL)
					yval_grid *= KM2DEG;
				if (arrival[narr].sheetdesc.buffer == NULL) {
					/* read time grid from disk */
					fp_grid = arrival[narr].fpgrid;
					ptgrid = &(arrival[narr].gdesc);
				} else {
					/* read time grid from memory buffer */
					fp_grid = NULL;
					ptgrid = &(arrival[narr].sheetdesc);
				}
				if ((arrival[narr].pred_travel_time =
						ReadAbsInterpGrid2d(fp_grid, ptgrid, yval_grid, zval)) < 0.0)
					nReject++;
	//printf("getTT:  xval %lf yval %lf yval_grid %lf zval %lf t %lf \n", xval, yval, yval_grid, zval, arrival[narr].pred_travel_time);
	//display_grid_param(&(arrival[narr].sheetdesc));
			}
			arrival[narr].pred_travel_time *= arrival[narr].tfact;
			// apply crustal correction
			if (ApplyCrustElevCorrFlag  && GeometryMode == MODE_GLOBAL
						 && arrival[narr].pred_travel_time > 0.0) {
//printf("arrival[narr].pred_travel_time before: %f", arrival[narr].pred_travel_time);
				if (yval_grid > MinDistCrustElevCorr)
					arrival[narr].pred_travel_time +=
							applyCrustElevCorrection(arrival + narr, xval, yval, zval);
//printf(" -> after: %f\n", arrival[narr].pred_travel_time);
						 }
						 else if (ApplyElevCorrFlag) {
							 if (arrival[narr].pred_travel_time > 0.0) // ignore arrivals with no pred tt
								 arrival[narr].pred_travel_time += arrival[narr].elev_corr;
						 }
		}
	}

	return(nReject);

}



/** function to apply crustal correction and elevation correction */

// assumes vertical ray (dtdd = 0.0) !!!

double applyCrustElevCorrection(ArrivalDesc* parrival, double xval, double yval, double zval) {

	double correction = 0.0;
	double dtdd = 0.0;
	char cphase;

	if (IsPhaseID(parrival->phase, "P"))
		cphase = 'P';
	else if (IsPhaseID(parrival->phase, "S"))
		cphase = 'S';
	else
		return(0.0);


// source
	correction = calc_crust_corr(cphase, yval, xval, zval, VERY_LARGE_DOUBLE, dtdd);
// receiver
	correction +=
			calc_crust_corr(cphase, parrival->station.dlat,
					parrival->station.dlong, 0.0, -1000.0 * parrival->station.depth, dtdd);

	return(correction);

}



/** function to check if xyz location is above topography */

int isAboveTopo(double xval, double yval, double zval) {

	double dlong, dlat, elev;
	double topo_elev;

	// check topo if available
	if (topo_surface_index >= 0) {
		// convert xyz to lat/long/elev
		rect2latlon(0, xval, yval, &dlat, &dlong);
		elev = -zval * 1000.0;	// elevation in meters
		// get elevation of topo at location
		topo_elev = get_surface_z(topo_surface_index, dlong, dlat);
//printf("xyz %f %f %f  lon/lat/elev %f %f %f  topo_elev %f  above %d\n", xval, yval, zval, dlat, dlong, elev, topo_elev, elev > topo_elev);
		if (elev > topo_elev)
			return(1);
	}

	return(0);

}





/*------------------------------------------------------------/ */
/** Octree search routines */



/** function to initialize Octree search */

Tree3D* InitializeOcttree(GridDesc* ptgrid,  OcttreeParams* pParams)
{

	double dx, dy, dz;
	Tree3D* newTree;
	void *pdata = NULL;
	double integral;

	// set up octree x, y, z grid
	dx = ptgrid->dx * (double) (ptgrid->numx - 1) / (double) (pParams->init_num_cells_x);
	dy = ptgrid->dy * (double) (ptgrid->numy - 1) / (double) (pParams->init_num_cells_y);
	dz = ptgrid->dz * (double) (ptgrid->numz - 1) / (double) (pParams->init_num_cells_z);

	integral = 0.0;
	newTree = newTree3D(ptgrid->type, pParams->init_num_cells_x,
			    pParams->init_num_cells_y, pParams->init_num_cells_z,
			    ptgrid->origx, ptgrid->origy, ptgrid->origz,
			    dx, dy, dz, OCTREE_UNDEF_VALUE, integral, pdata);

	return(newTree);
}



/** function to perform Octree location */

int LocOctree(int ngrid, int num_arr_total, int num_arr_loc,
	      ArrivalDesc *arrival,
	      GridDesc* ptgrid, GaussLocParams* gauss_par, HypoDesc* phypo,
	      OcttreeParams* pParams, Tree3D* pOctTree, float* fdata,
	      double *poct_node_value_max, double *poct_tree_integral)
{

	int nSamples, narr, ipos;
	int nInitial;
	int iGridType;
	//int nReject;
	int iReject = 0;
	int iBoundary = 0;
	double xval, yval, zval;

	long double value, dlike, value_max = (long double) -VERY_LARGE_DOUBLE;
	double misfit;
	double misfit_min = VERY_LARGE_DOUBLE, misfit_max = -VERY_LARGE_DOUBLE;
	double hypo_dx = -1.0, hypo_dz = -1.0;

	int nScatterSaved;

	int ix, iy, iz;
	double logWtMtrxSum;
	//double volume, log_value_volume;
	int icalc_cell_diagonal_time_var = 0;
	double diagonal, cell_diagonal_time_var, volume_min;
	//double dsx, dsy, dsz;
	//double dsx_global, dsy_global, depth_corr;
	ResultTreeNode* presult_node;
	OctNode* poct_node;
	OctNode* pparent_oct_node;

	int n_neigh;
	OctNode* neighbor_node;
	Vect3D coords;
	double min_node_size_x;
	double min_node_size_y;
	double min_node_size_z;
	double smallest_node_size_x = -1.0;
	double smallest_node_size_y = -1.0;
	double smallest_node_size_z = -1.0;

	//double stationDensityWeight = 0.0;
	//double value_search = -1.0;



	// reset EDT_otime_weight_active flag
	EDT_otime_weight_active = 0;

	// cell diagonal variance defaults to null - i.e. no effect
	diagonal = cell_diagonal_time_var = 0.0;
	icalc_cell_diagonal_time_var = pParams->mean_cell_velocity > 0.0;
	volume_min = VERY_LARGE_DOUBLE;


	iGridType = GRID_PROB_DENSITY;

	if (message_flag >= 4) {
		putmsg(4, "");
		putmsg(4, "Calculating solution in Octree...");
	}

	logWtMtrxSum = log(gauss_par->WtMtrxSum);

	// set min node size (in km)
	min_node_size_x = min_node_size_y = min_node_size_z = pParams->min_node_size;
	// following neglects convergence of longitude towards the poles and convergence with depth
	if (GeometryMode == MODE_GLOBAL)
		min_node_size_x = min_node_size_y = pParams->min_node_size * KM2DEG;

	/* first get solutions at each cell in Tree3D */

	nSamples = 0;
	resultTreeRoot = NULL;
	for (ix = 0; ix < pOctTree->numx; ix++) {
		for (iy = 0; iy < pOctTree->numy; iy++) {
			for (iz = 0; iz < pOctTree->numz; iz++) {
				poct_node = pOctTree->nodeArray[ix][iy][iz];
// $$$ NOTE: this block must be identical to block $$$ below
				xval = poct_node->center.x;
				yval = poct_node->center.y;
				zval = poct_node->center.z;
				value = LocOctree_core(ngrid, xval, yval, zval, num_arr_loc, arrival, poct_node, icalc_cell_diagonal_time_var, &volume_min, &diagonal, &cell_diagonal_time_var, pParams, gauss_par, iGridType, &misfit, logWtMtrxSum);
				nSamples++;
// END - this block must be identical to block $$$ below

				// save node size
				smallest_node_size_x = poct_node->ds.x;
				smallest_node_size_y = poct_node->ds.y;
				smallest_node_size_z = poct_node->ds.z;

				if (message_flag >= 1 && nSamples % 5000 == 0) {
					fprintf(stdout,
							"OctTree num samples = %d / %d\r", nSamples, pParams->max_num_nodes);
					fflush(stdout);
				}

			}
		}
	}
	nInitial = nSamples;


	/* loop over octree nodes */

	nScatterSaved = 0;
	ipos = 0;

	while (nSamples < pParams->max_num_nodes) {

		if (pParams->stop_on_min_node_size)
			presult_node = getHighestLeafValue(resultTreeRoot);
		else
			presult_node = getHighestLeafValueMinSize(resultTreeRoot,
				min_node_size_x, min_node_size_y, min_node_size_z);
		// check if null node
		if (presult_node == NULL) {
			if (message_flag >= 1)
				fprintf(stdout, "\nINFO: No more nodes larger than min_node_size, terminating Octree search.");
			break;
		}


//if (nSamples % 100 == 0)
//fprintf(stderr, "%d getHighestLeafValue %lf\n", nSamples, presult_node->value);

		if (presult_node == NULL)
			fprintf(stderr, "\npresult_node == NULL!!\n");

		pparent_oct_node = presult_node->pnode;

		// subdivide all HighestLeafValue neighbors

		for (n_neigh = 0; n_neigh < 7; n_neigh++) {

			if (n_neigh == 0) {
				neighbor_node = pparent_oct_node;
			} else {
				coords.x = pparent_oct_node->center.x;
				coords.y = pparent_oct_node->center.y;
				coords.z = pparent_oct_node->center.z;
				if (n_neigh == 1) {
					coords.x = pparent_oct_node->center.x
							+ (pparent_oct_node->ds.x + smallest_node_size_x) / 2.0;
				} else if (n_neigh == 2) {
					coords.x = pparent_oct_node->center.x
							- (pparent_oct_node->ds.x + smallest_node_size_x) / 2.0;
				} else if (n_neigh == 3) {
					coords.y = pparent_oct_node->center.y
							+ (pparent_oct_node->ds.y + smallest_node_size_y) / 2.0;
				} else if (n_neigh == 4) {
					coords.y = pparent_oct_node->center.y
							- (pparent_oct_node->ds.y + smallest_node_size_y) / 2.0;
				} else if (n_neigh == 5) {
					coords.z = pparent_oct_node->center.z
							+ (pparent_oct_node->ds.z + smallest_node_size_z) / 2.0;
				} else if (n_neigh == 6) {
					coords.z = pparent_oct_node->center.z
							- (pparent_oct_node->ds.z + smallest_node_size_z) / 2.0;
				}
				// check for longitude wrap-around
				if (GeometryMode == MODE_GLOBAL) {
					if (coords.x > 180.0)
						coords.x -= 360.0;
					if (coords.x < -180.0)
						coords.x += 360.0;
				}

				// find neighbor node
				neighbor_node = getLeafNodeContaining(pOctTree, coords);
				// outside of octTree volume
				if (neighbor_node == NULL)
					continue;
				// already subdivided
				if (neighbor_node->ds.x < 0.99 * pparent_oct_node->ds.x)
					continue;
			}


			// subdivide node and evaluate solution at each child
			subdivide(neighbor_node, OCTREE_UNDEF_VALUE, NULL);

			for (ix = 0; ix < 2; ix++) {
				for (iy = 0; iy < 2; iy++) {
					for (iz = 0; iz < 2; iz++) {

						poct_node = neighbor_node->child[ix][iy][iz];

//if (poct_node->ds.x < pParams->min_node_size || poct_node->ds.y < pParams->min_node_size || poct_node->ds.z < pParams->min_node_size)
//fprintf(stderr, "\nnode size too small!! %lf %lf %lf\n", poct_node->ds.x, poct_node->ds.y, poct_node->ds.z);

						// save node size if smallest so far
						if (poct_node->ds.x < smallest_node_size_x)
							smallest_node_size_x = poct_node->ds.x;
						if (poct_node->ds.y < smallest_node_size_y)
							smallest_node_size_y = poct_node->ds.y;
						if (poct_node->ds.z < smallest_node_size_z)
							smallest_node_size_z = poct_node->ds.z;

// $$$ NOTE: this block must be identical to block $$$ above
						xval = poct_node->center.x;
						yval = poct_node->center.y;
						zval = poct_node->center.z;
						value = LocOctree_core(ngrid, xval, yval, zval, num_arr_loc, arrival, poct_node, icalc_cell_diagonal_time_var, &volume_min, &diagonal, &cell_diagonal_time_var, pParams, gauss_par, iGridType, &misfit, logWtMtrxSum);
						nSamples++;
// END - this block must be identical to block $$$ above

						if (message_flag >= 1 && nSamples % 5000 == 0) {
							fprintf(stdout,
									"OctTree num samples = %d / %d\r", nSamples, pParams->max_num_nodes);
							fflush(stdout);
						}

						/* check for maximum likelihood */
						if (value > value_max) {
							value_max = value;
							misfit_min = misfit;
							phypo->misfit = misfit;
							phypo->x = xval;
							phypo->y = yval;
							phypo->z = zval;
							hypo_dx = poct_node->ds.x;
							hypo_dz = poct_node->ds.z;
							for (narr = 0; narr < num_arr_loc; narr++)
								arrival[narr].pred_travel_time_best = arrival[narr].pred_travel_time;
							*poct_node_value_max = poct_node->value;
						}
						if (misfit > 0.0 && misfit > misfit_max)	// misfit < 0 for topo masking
							misfit_max = misfit;


/* set to TRUE to save all samples, REMEMBER to set OCT num_scatter high enough in control file */
						if (0) {
							/* save sample to scatter file */
							fdata[ipos++] = xval;
							fdata[ipos++] = yval;
							fdata[ipos++] = zval;
							dlike = (long double) gauss_par->WtMtrxSum * (long double) exp(value);
							fdata[ipos++] = dlike;

							/* update  probabilitic residuals */
							if (1)
								UpdateProbabilisticResiduals(num_arr_loc, arrival, 1.0);

							nScatterSaved++;
						}


					} // end triple loop over node children
				}
			}

		}  // end loop over HighestLeafValue neighbors

		// check if minimum node size reached
		if (pParams->stop_on_min_node_size && (smallest_node_size_x < min_node_size_x
				  || smallest_node_size_y < min_node_size_y
				  || smallest_node_size_z < min_node_size_z)) {
			if (message_flag >= 1)
				fprintf(stdout, "\nINFO: Min node size reached, terminating Octree search.");
			break;
				  }

	}  // end while (nSamples < pParams->max_num_nodes)

	if (message_flag >= 1)
		fprintf(stdout, "\n");


	/* give warning if ... */
/*
	if () {
	sprintf(MsgStr,
	"WARNING: %d Metropolis samples rejected; travel times for an average of %.2lf arrival observations were not valid.",
	numGridReject, (double) numStaReject / numGridReject);
	putmsg(1, MsgStr);
}
*/

	/* check reject location conditions */

	/* maximum like hypo on edge of grid */

	if ((iBoundary = isOnGridBoundary(phypo->x, phypo->y, phypo->z, ptgrid, hypo_dx, hypo_dz, 0))) {
		sprintf(MsgStr,
			"WARNING: max prob location on grid boundary %d, rejecting location.", iBoundary);
		putmsg(1, MsgStr);
		sprintf(phypo->locStatComm, "%s", MsgStr);
		iReject = 1;
	}

	// determine integral of all oct-tree leaf node pdf values
	*poct_tree_integral = integrateResultTree(resultTreeRoot, 0.0, *poct_node_value_max);
	sprintf(MsgStr, "Octree oct_node_value_max= %le oct_tree_integral= %le", *poct_node_value_max, *poct_tree_integral);
	putmsg(1, MsgStr);



	// construct search information string
	if (GeometryMode == MODE_GLOBAL) {
		smallest_node_size_x *= DEG2KM;
		smallest_node_size_y *= DEG2KM;
	}
	sprintf(phypo->searchInfo, "OCTREE nInitial %d nEvaluated %d smallestNodeSide %lf/%lf/%lf oct_tree_integral %le",
		nInitial, nSamples, smallest_node_size_x, smallest_node_size_y, smallest_node_size_z, *poct_tree_integral);
	/* write message */
	putmsg(2, phypo->searchInfo);


	/* check for termination */
	if (iReject) {
		sprintf(Hypocenter.locStat, "REJECTED");
	}


	/* re-calculate solution and arrival statistics for best location */

	SaveBestLocation(num_arr_total, num_arr_loc, arrival,  ptgrid,
			 gauss_par, phypo, misfit_max, iGridType);


	return(nScatterSaved);

}



/** function to perform Octree core solution evaluation */

INLINE long double LocOctree_core(int ngrid, double xval, double yval, double zval,
		int num_arr_loc, ArrivalDesc *arrival,
		OctNode* poct_node,
		int icalc_cell_diagonal_time_var, double *volume_min,
		double *diagonal, double *cell_diagonal_time_var,
		OcttreeParams* pParams, GaussLocParams* gauss_par, int iGridType,
		double *misfit, double logWtMtrxSum)
{

	long double value;

	int iAboveTopo;
	int nReject;
	double volume, log_value_volume;
	double dsx, dsy, dsz;
	double dsx_global, dsy_global, depth_corr;
	double stationDensityWeight = 0.0;
	double value_search = -1.0;


	/* get travel times for observed arrivals */
	iAboveTopo = isAboveTopo(xval, yval, zval);
	if (!iAboveTopo) {
		nReject = getTravelTimes(arrival, num_arr_loc, xval, yval, zval);
		if (nReject && GeometryMode != MODE_GLOBAL) {
			sprintf(MsgStr,
				"ERROR: octree sample at (%lf,%lf,%lf) is outside of %d travel time grids.",
				xval, yval, zval, nReject);
			puterr(MsgStr);
		}
	}

	// calculate cell volume and diagonal
	dsx = poct_node->ds.x;
	dsy = poct_node->ds.y;
	dsz = poct_node->ds.z;
	if (GeometryMode == MODE_GLOBAL) {
		depth_corr = (ERAD - poct_node->center.z) / ERAD;
		dsx_global = dsx * DEG2KM * cos(DE2RA * poct_node->center.y) * depth_corr;
		dsy_global = dsy * DEG2KM * depth_corr;
		volume = dsx_global * dsy_global * dsz;
		if (icalc_cell_diagonal_time_var) {
			//if (volume < *volume_min)
			*volume_min = volume;
			*diagonal = pow(*volume_min, 0.33333333);
			//*diagonal = sqrt(dsx_global * dsx_global * dsy_global * dsy_global + dsz * dsz);
			//*diagonal = dsx_global < dsy_global ? dsx_global : dsy_global;
			//*diagonal = dsz < *diagonal ? dsz : *diagonal;
		}
	} else {
		volume = dsx * dsy * dsz;
		if (icalc_cell_diagonal_time_var) {
			//if (volume < *volume_min)
			*volume_min = volume;
			*diagonal = pow(*volume_min, 0.33333333);
			//*diagonal = sqrt(dsx * dsx + dsy * dsy + dsz * dsz);
		}
	}
	if (icalc_cell_diagonal_time_var) {
		*cell_diagonal_time_var = 0.5 * *diagonal / pParams->mean_cell_velocity;
		*cell_diagonal_time_var *= *cell_diagonal_time_var;
	}
	// calc misfit and prob density
//EDT_use_otime_weight = 0;
	if (!iAboveTopo) {		// above topo
		value = CalcSolutionQuality(num_arr_loc, arrival, gauss_par, iGridType, misfit, NULL, NULL, *cell_diagonal_time_var, &value_search);
	} else {
		value = -VERY_LARGE_DOUBLE;
		*misfit = -VERY_LARGE_DOUBLE;
	}
						//if (!icalc_cell_diagonal_time_var)
	value_search = value;
//EDT_use_otime_weight = 1;
	// calculate cell volume * prob density and put cell in resultTree
	log_value_volume =  logWtMtrxSum + value_search + log(volume);
	poct_node->value = logWtMtrxSum + value;
	if (pParams->use_stations_density) {
		stationDensityWeight =
				getOctTreeStationDensityWeight(poct_node, StationList, NumStations, LocGrid + ngrid, pParams->use_stations_density);
							// AJL 20060531 bug fix ? - changed *= to +=
		log_value_volume += stationDensityWeight;
		poct_node->value += stationDensityWeight;
	}

	resultTreeRoot = addResult(resultTreeRoot, log_value_volume, volume, poct_node);

	return(value);

}



/** function to calculate (logarithmic) station density weight value for an oct tree node */

double getOctTreeStationDensityWeight_OLD1(OctNode* poct_node, SourceDesc *stations, int numStations, GridDesc *ptgrid)
{

	int n, n_inside, numStations_this_event;
	double staDensityWeight;
	SourceDesc *station;

	// check if parent node contains no stations

	if (poct_node->parent != NULL
		   && poct_node->parent->pdata != NULL && *((int *) poct_node->parent->pdata) <= 1)
		return(1.0);

	// count number of stations inside this node
	numStations_this_event = 0;
	n_inside = 0;
	for (n = 0; n < numStations; n++) {
		station = stations + n;
		// check if station has not-ignored reading for this event
		if (station->ignored)
			continue;
		numStations_this_event++;
		// check if station location is unknown
		if (station->x <= -LARGE_DOUBLE)
			continue;
		// check if station is above top of grid
		if (station->z < ptgrid->origz) {
			if (extendedNodeContains(poct_node, station->x, station->y, ptgrid->origz, 0))
				n_inside++;
		} else {
			if (extendedNodeContains(poct_node, station->x, station->y, station->z, 0))
				n_inside++;
		}
//printf("n %d  node %f %f %f  stations %f %f %f  ptgrid->origz %f\n", n, poct_node->center.x, poct_node->center.y, poct_node->center.z, station->x, station->y, station->z, ptgrid->origz);
	}

	if (poct_node->pdata == NULL)
		poct_node->pdata = (void *) malloc(sizeof(int));
	if (poct_node->pdata != NULL)
		*((int *) poct_node->pdata) = n_inside;
	else
		puterr("ERROR: allocating int storage for OctTree Station Density Weight count.");

	staDensityWeight = log((double) (n_inside + 1));

//if (n_inside > 0) printf("OctTreeStationDensityWeight: node %f %f %f  n_inside %d  numStations_this_event %d\n", poct_node->center.x, poct_node->center.y, poct_node->center.z, n_inside, numStations_this_event);

	return(staDensityWeight);

}



/** function to calculate (logarithmic) station density weight value for an oct tree node */

double getOctTreeStationDensityWeight_OLD2(OctNode* poct_node, SourceDesc *stations, int numStations, GridDesc *ptgrid)
{

	int n, n_inside, numStations_this_event;
	double staDensityWeight;
	SourceDesc *station;


	n_inside = 0;

	// check if parent node contains stations data
	if (poct_node->parent != NULL)
	{
		// this must be a child node, parent should have station count data
		if (poct_node->parent->pdata == NULL) {	// should not get here
			puterr("ERROR: parent node exists but has no OctTree Station Density Weight count!");
		}
		else
		{
			n_inside = *((int *) poct_node->parent->pdata);
		}
	}
	else
	{
		// this must be a root node
		// count number of stations inside this node
		numStations_this_event = 0;
		n_inside = 0;
		for (n = 0; n < numStations; n++) {
			station = stations + n;
			// check if station has not-ignored reading for this event
			if (station->ignored)
				continue;
			numStations_this_event++;
			// check if station location is unknown
			if (station->x <= -LARGE_DOUBLE)
				continue;
			// check if station is above top of grid
			if (station->z < ptgrid->origz) {
				if (extendedNodeContains(poct_node, station->x, station->y, ptgrid->origz, 0)) {
					n_inside++;
//printf("n %d  node %f %f %f  stations %f %f %f  ptgrid->origz %f\n", n, poct_node->center.x, poct_node->center.y, poct_node->center.z, station->x, station->y, station->z, ptgrid->origz);
				}
			} else {
				if (extendedNodeContains(poct_node, station->x, station->y, station->z, 0)) {
					n_inside++;
//printf("n %d  node %f %f %f  stations %f %f %f  ptgrid->origz %f\n", n, poct_node->center.x, poct_node->center.y, poct_node->center.z, station->x, station->y, station->z, ptgrid->origz);
				}
			}
		}
//if (n_inside > 0) printf("OctTreeStationDensityWeight: node %f %f %f  n_inside %d  numStations_this_event %d\n", poct_node->center.x, poct_node->center.y, poct_node->center.z, n_inside, numStations_this_event);
	}

	if (poct_node->pdata == NULL)
		poct_node->pdata = (void *) malloc(sizeof(int));
	if (poct_node->pdata != NULL)
		*((int *) poct_node->pdata) = n_inside;
	else
		puterr("ERROR: allocating int storage for OctTree Station Density Weight count.");

	staDensityWeight = log((double) (n_inside + 1));

	// multiply by arbitrary constant... (seems to work!)
	//staDensityWeight *= 10.0;
	staDensityWeight *= 2.0;

	return(staDensityWeight);

}



/** function to calculate (logarithmic) station density weight value for an oct tree node */

double getOctTreeStationDensityWeight(OctNode* poct_node, SourceDesc *stations, int numStations, GridDesc *ptgrid,
				      int iOctLevelMax)
{

	int node_level, n, numStations_this_event;
	double log_station_density_weight, return_weight;
	SourceDesc *station;
	double epi_dist, depth_diff, hypo_dist, hypo_dist_min, cut_off_dist;
	double x_node_cent, y_node_cent, z_node_cent, mean_node_horiz_ds;
	OctNode* pnode;

	static double mean_root_node_horiz_ds = -VERY_LARGE_DOUBLE;
	// !!! shoud be initialized for each event????



	if (mean_root_node_horiz_ds == -VERY_LARGE_DOUBLE) {
		pnode = poct_node;
		while(pnode->parent != NULL)
			pnode = pnode->parent;
		mean_root_node_horiz_ds = (pnode->ds.x + pnode->ds.y);
		if (GeometryMode == MODE_GLOBAL)
			mean_root_node_horiz_ds *= DEG2KM;
		sprintf(MsgStr, "Station Density Weight:  Mean Root Node Horiz dS: %lf", mean_root_node_horiz_ds);
		putmsg(1, MsgStr);
	}
	if (mean_root_node_horiz_ds < SMALL_DOUBLE) {	// should not get here
		puterr("ERROR: cannot apply OctTree Station Density Weight: Mean Root Node Horiz dS is zero!");
	}



	log_station_density_weight = 0.0;
	return_weight = log_station_density_weight;

	// get level of this node in tree
	node_level = 0;
	pnode = poct_node;
	while(pnode->parent != NULL) {
		pnode = pnode->parent;
		node_level++;
	}


	// node level dependent calculation
	if (node_level >= iOctLevelMax)
	{
		// node above max level, get data from parent
		if (poct_node->parent->pdata == NULL) {	// should not get here
			puterr("ERROR: parent node exists but has no OctTree Station Density Weight value!");
		} else {
			log_station_density_weight = *((double *) poct_node->parent->pdata);
		}
		return_weight = log_station_density_weight;
		//TESTreturn_weight = 0.0;
	}
	else
	{
		// node below max level
		// calcualte average station distance from center of this node
		x_node_cent = poct_node->center.x;
		y_node_cent = poct_node->center.y;
		z_node_cent = poct_node->center.z;
		numStations_this_event = 0;
		hypo_dist_min = VERY_LARGE_DOUBLE;
		for (n = 0; n < numStations; n++) {
			station = stations + n;
			// check if station has ignored reading for this event
			if (station->ignored)
				continue;
			numStations_this_event++;
			// check if station location is unknown
			if (station->x <= -LARGE_DOUBLE)
				continue;
			// get distance from station to center of node
			epi_dist = GetEpiDist(station, x_node_cent, y_node_cent);
			depth_diff = z_node_cent - station->z;
			hypo_dist = sqrt(epi_dist * epi_dist + depth_diff * depth_diff);
			hypo_dist_min = hypo_dist < hypo_dist_min ? hypo_dist : hypo_dist_min;
//printf("n %d  node %f %f %f  stations %f %f %f  ptgrid->origz %f\n", n, poct_node->center.x, poct_node->center.y, poct_node->center.z, station->x, station->y, station->z, ptgrid->origz);
		}
		//if (ndist == 0) {	// should not get here
		//	puterr("ERROR: no stations found for OctTree Station Density Weight calculation!");
		//}
		if (hypo_dist_min > VERY_SMALL_DOUBLE) {
			mean_node_horiz_ds = (poct_node->ds.x + poct_node->ds.y);
			if (GeometryMode == MODE_GLOBAL)
				mean_node_horiz_ds *= DEG2KM;
			cut_off_dist = mean_node_horiz_ds > AveInterStationDistance
					? mean_node_horiz_ds : AveInterStationDistance;
			/*
			if (hypo_dist_min <= cut_off_dist)
			log_station_density_weight = 1.0;
			else
			log_station_density_weight = cut_off_dist / hypo_dist_min;
			//log_station_density_weight *= mean_root_node_horiz_ds / AveInterStationDistance;
			log_station_density_weight *= 10.0;
			//	if (log_station_density_weight > 10.0)
			//		log_station_density_weight = 10.0;
			*/
			log_station_density_weight = hypo_dist_min / cut_off_dist;
			log_station_density_weight = -log_station_density_weight * log_station_density_weight;
			//log_station_density_weight *= mean_root_node_horiz_ds / AveInterStationDistance;
			//log_station_density_weight *= 10.0;
			//	if (log_station_density_weight > 10.0)
			//		log_station_density_weight = 10.0;
//if (log_station_density_weight >= -0.5) printf("OctTreeStationDensityWeight: node %f %f %f  sta_den_wt %f  numStations_this_event %d  AveInterStationDistance %f  hypo_dist_min %f  mean_node_horiz_ds %f  node_level %d\n", poct_node->center.x, poct_node->center.y, poct_node->center.z, exp(log_station_density_weight), numStations_this_event, AveInterStationDistance, hypo_dist_min, mean_node_horiz_ds, node_level);
			// force cell division
			if (node_level < iOctLevelMax && hypo_dist_min < cut_off_dist) {
				return_weight =  (double) (iOctLevelMax - node_level);
				return_weight = return_weight * return_weight;
				NumForceOctTreeStaDenWt++;
			} else {
				return_weight = log_station_density_weight;
			}
		}
	}

	if (poct_node->pdata == NULL)
		poct_node->pdata = (void *) malloc(sizeof(double));
	if (poct_node->pdata != NULL)
		*((double *) poct_node->pdata) = log_station_density_weight;
	else
		puterr("ERROR: allocating int storage for OctTree Station Density Weight count.");

	// multiply by arbitrary constant... (seems to work!)
	//staDensityWeight *= 10.0;
	//log_station_density_weight *= 10.0;

	return(return_weight);

}



/** function to generate sample (scatter) of OctTree results */

int GenEventScatterOcttree(OcttreeParams* pParams, double oct_node_value_max, float* fscatterdata, double integral, HypoDesc* phypo)
{

	int tot_npoints;
	int fdata_index;
	double oct_tree_scatter_volume;
	char scatter_volume_text[32];


	oct_tree_scatter_volume = 0.0;

	/* return if no scatter samples requested */
	if (pParams->num_scatter < 1)
		return(0);

	/* write message */
	if (message_flag >= 3) {
		putmsg(3, "");
		putmsg(3, "Generating event scatter file...");
	}


	/* generate scatter points at uniformly-randomly chosen locations in each leaf node */

	tot_npoints = 0;
	fdata_index = 0;
	tot_npoints = getScatterSampleResultTree(resultTreeRoot, pParams, integral,
			fscatterdata, tot_npoints, &fdata_index, oct_node_value_max, &oct_tree_scatter_volume);

	/* write message */
	if (message_flag >= 3) {
		sprintf(MsgStr, "  %d points generated, %d points requested, oct_tree_scatter_volume= %le",
			tot_npoints, pParams->num_scatter, oct_tree_scatter_volume);
		putmsg(3, MsgStr);
	}

	// update hypocenter searchInfo
	sprintf(scatter_volume_text, " scatter_volume %le", oct_tree_scatter_volume);
	strcat(phypo->searchInfo, scatter_volume_text);

	return(tot_npoints);

}


/** function to integrate exp(val) * volume of all leafs in results tree */

double integrateResultTree(ResultTreeNode* prtree, double sum, double oct_node_value_max)
{
	OctNode* pnode;

	if (prtree->left != NULL)
		sum = integrateResultTree(prtree->left, sum, oct_node_value_max);

	pnode = prtree->pnode;
	if (pnode->isLeaf) {
		// result tree value is log(value + volume)
		// AJL 20061023 bug fix - prtree value may not be same as pnode value * volume
		//sum += exp(prtree->value - oct_node_value_max);
		sum += exp(pnode->value - oct_node_value_max) * prtree->volume;
	}

	if (prtree->right != NULL)
		sum = integrateResultTree(prtree->right, sum, oct_node_value_max);

	return(sum);
}



/** function to normalize value of all leafs in results tree to range 0.0-1.0 */

double convertOcttreeValuesToProb(ResultTreeNode* prtree, double integral, double oct_node_value_max)
{
	OctNode* pnode;


	if (prtree->left != NULL)
		integral = convertOcttreeValuesToProb(prtree->left, integral, oct_node_value_max);

	pnode = prtree->pnode;
	if (pnode->isLeaf) {
		pnode->value = exp(pnode->value - oct_node_value_max);	// replace leaf value with relative prob density
		integral += pnode->value * prtree->volume;	// integrate value * cell volume
	}

	if (prtree->right != NULL)
		integral = convertOcttreeValuesToProb(prtree->right, integral, oct_node_value_max);

	return(integral);

}


/** function to create a new ResultTree using node values */

ResultTreeNode* createResultTree(ResultTreeNode* prtree, ResultTreeNode* pnew_rtree)
{
	OctNode* pnode;


	if (prtree->left != NULL)
		pnew_rtree = createResultTree(prtree->left, pnew_rtree);

	pnode = prtree->pnode;
	if (pnode->isLeaf) {
		pnew_rtree = addResult(pnew_rtree, pnode->value, prtree->volume, pnode);
	}

	if (prtree->right != NULL)
		pnew_rtree = createResultTree(prtree->right, pnew_rtree);

	return(pnew_rtree);

}



/** function to get scatter sample for all leafs in results tree */

int getScatterSampleResultTree(ResultTreeNode* prtree, OcttreeParams* pParams,
			       double integral, float* fdata, int npoints, int* pfdata_index,
			       double oct_node_value_max, double *poct_tree_scatter_volume)
{
	OctNode* pnode;
	double xnpoints, xval, yval, zval;
	double dx, dy, dz;
	int isample_taken;

	if (prtree->right != NULL)
		npoints = getScatterSampleResultTree(prtree->right, pParams, integral,
			fdata, npoints, pfdata_index, oct_node_value_max, poct_tree_scatter_volume);

	pnode = prtree->pnode;
	if (pnode->isLeaf /*&& npoints < pParams->num_scatter*/) {

		// AJL 20061023 bug fix - prtree value may not be same as pnode value * volume
		//xnpoints = (double) pParams->num_scatter * exp(prtree->value - oct_node_value_max) / integral;
		xnpoints = (double) pParams->num_scatter
				* (exp(pnode->value - oct_node_value_max) * prtree->volume) / integral;

		xval = pnode->center.x;
		yval = pnode->center.y;
		zval = pnode->center.z;
		dx = pnode->ds.x / 2.0;
		dy = pnode->ds.y / 2.0;
		dz = pnode->ds.z / 2.0;

		isample_taken = 0;

		while (xnpoints > 0.0 /*&& npoints < pParams->num_scatter*/) {

			if (xnpoints > 1.0 || xnpoints - (double) ((int) xnpoints) > get_rand_double(0.0, 1.0)) {
				fdata[*pfdata_index + 0] = xval + get_rand_double(-dx, dx);
//printf("npoints %d  *pfdata_index %d  %lf  dx %lf  exp(prtree->value) %le  integral %le\n", npoints, *pfdata_index, fdata[*pfdata_index + 0], dx, exp(prtree->value), integral);
				fdata[*pfdata_index + 1] = yval + get_rand_double(-dy, dy);
				fdata[*pfdata_index + 2] = zval + get_rand_double(-dz, dz);
				fdata[*pfdata_index + 3] = pnode->value;
				npoints++;
				isample_taken = 1;
				*pfdata_index += 4;
			}

			xnpoints -= 1.0;

		}

		// update wieghted scatter volume
		// pnode->value is probability density
		// oct_node_value_max is maximum probability density
		*poct_tree_scatter_volume += prtree->volume * exp(pnode->value - oct_node_value_max);
	}

	if (prtree->left != NULL)
		npoints = getScatterSampleResultTree(prtree->left, pParams, integral,
			fdata, npoints, pfdata_index, oct_node_value_max, poct_tree_scatter_volume);

	return(npoints);
}






/** end of Octree search routines */
/*------------------------------------------------------------/ */



