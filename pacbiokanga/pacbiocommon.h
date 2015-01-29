#pragma once
// Copyright 2013 CSIRO  ( http://www.csiro.au/ ) 
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License
//   Please contact stuart.stephen@csiro.au for support or 
//   to submit modifications to this source


const int cMinSeedCoreLen = 12;							// user can specify seed cores down to this minimum length
const int cDfltSeedCoreLen = 16;						// default seed cores of this length
const int cMaxSeedCoreLen = 25;							// user can specify seed cores up to to this maximum length
const int cMinNumSeedCores = 3;							// user can specify requiring at least this many seed cores between overlapping scaffold sequences
const int cDfltNumSeedCores = 5;						// default is to require at least this many seed cores between overlapping scaffold sequences
const int cMaxNumSeedCores = 25;						// user can specify requiring up to many seed cores between overlapping scaffold sequences
const int cAnchorLen = 10;								// require 5' and 3' end anchors of at least this length for overlap merging
const int cMaxHitsPerSeedCore = 2000;					// limit number of hits from any seed core onto other sequences to be this
const int cQualCoreKMerLen = 3;							// using trimers when checking for core downstream shared kmers between probe and target. Note currently a max of 4 would be supported as any more would violate cQualCoreDelta constraint 
const int cQualCoreDelta = (cQualCoreKMerLen * 2) + 1;	 // looking for matching trimers starting within +/- 7bp of the probe trimer. NOTE must be <= cMinSeedCoreLen
const int cQualCoreThres = 25;							 // require at least this many kmers to be shared between probe and target before accepting core
const int cQualCoreHomopolymer = 80;                     // if any core contains more than this percentage of the same base then treat as being a near homopolymer core (likely a PacBio insert) and slough

const int cMinScaffSeqLen = 100;						 // allowing for minimum scaffolded sequences to be specified down to this length (could be targeting RNA transcriptome)
const int cDfltMinScaffSeqLen = 5000;					 // default is to allow for scaffolded sequences down to this minimum length
const int cMaxMinScaffSeqLen = 100000;					 // allowing for minimum scaffolded sequences to be specified up to this length 

const int cDfltSWMatchScore = 2;						// default SW match score for pacbio alignments
const int cDfltSWMismatchPenalty = -7;					// default SW mismatch penalty for pacbio alignments
const int cDfltSWGapOpenPenalty = -5;					// default SW gap open penalty for pacbio alignments
const int cDfltSWGapExtnPenalty = -2;					// default SW gap extension penalty for pacbio alignments
const int cDfltSWProgExtnLen = 3;						// default SW gap extension penalties apply with gaps of at least this size
const int cMaxAllowedSWScore = 50;						// allow SW scores or penalties to be specified up to this max

const int cMinSWPeakScore = 50;							// SW alignments must have peak scores of at least this
const int cMinSWAlignLen = 50;							// and alignment length of at least this many bases to be further processed


const int cMinOverlapLen = 1000;						 // minimum putative overlap length which can be requested by user
const int cDfltMinOverlapLen = 5000;					 // default minimum putative overlap length

const int cAllocdNumCoreHits = 1000000;					 // each thread preallocs for this many core hits, realloc'd as may be required
const int cAllocdQuerySeqLen = 500000;					 // each thread preallocs to hold query sequences of this length, realloc'd as may be required
const int cSummaryTargCoreHitCnts = 10;					 // summary core hit counts on at most this many targets

const int cChkOverlapGapLen = 20;						 // if gap between probe cores with at least one match > this threhold then set core probe offset at which to check for core extension (set 0 to disable core extensions)

const int cMaxWorkerThreads = 64;						// allow for at most 64 worker threads (will be clamped to the max available)