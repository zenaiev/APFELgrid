// The MIT License (MIT)

// Copyright (c) Stefano Carrazza, Luigi Del Debbio, Nathan Hartland

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <string>
#include <vector>
#include <istream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <cmath>

#include "fk_utils.h"
#include "exceptions.h"

namespace NNPDF
{

    static const bool Verbose = true;
    template<class T> static int convoluteAlign() { return 1; };
    template<> int convoluteAlign<float>() { return 4; }; // Doesn't hurt even without SSE

   /**
    * \class FKTable
    * \brief Class for holding FastKernel tables
    */
    template<typename T>
    class FKTable : public FKHeader 
    {
      public:
        FKTable(    std::istream& , 
                    std::vector<std::string> const& cFactors = std::vector<std::string>()
                ); // Stream constructor
        FKTable(    std::string const& filename,
                    std::vector<std::string> const& cfactors = std::vector<std::string>()
                ); //!< FK table reader

        FKTable(FKTable const&); //!< Copy constructor
        FKTable(FKTable const&, std::vector<int> const&); //!< Masked copy constructor

        virtual ~FKTable(); //!< Destructor
        void Print(std::ostream&); //!< Print FKTable header to ostream

        typedef void (*extern_pdf)(const double& x, const double& Q, const size_t& n, T* pdf);
        void Convolute(extern_pdf pdf, size_t const& NPDF, T* out);

        // ******************** FK Get Methods ***************************

        std::string const& GetDataName()  const {return fDataName;};

        double const&  GetQ20()      const { return fQ20;   }
        double const*  GetCFactors() const { return fcFactors; }

        int const&   GetNData()   const { return fNData;}  //!< Return fNData
        int const&   GetNx()      const { return fNx;   }  //!< Return fNx
        int const&   GetTx()      const { return fTx;   }  //!< Return fTx
        int const&   GetDSz()     const { return fDSz;  }  //!< Return fDSz
        int const&   GetPad()     const { return fPad;  }  //!< Return fPad

        double *  GetXGrid() const { return fXgrid;   }  //!< Return fXGrid
        T   *  GetSigma() const { return fSigma;   }  //!< Return fSigma

        int *   GetFlmap()   const { return fFlmap;   }  //!< Return fFlmap
        int const&   GetNonZero() const { return fNonZero; }  //!< Return fNonZero

        bool const& IsHadronic()  const { return fHadronic;}  //!< Return fHadronic

      protected:
        void ReadCFactors(std::string const& filename); //!< Read C-factors from file

        bool OptimalFlavourmap(std::string& flmap) const; //!< Determine and return the optimal flavour map

        // GetISig returns a position in the FK table
        int GetISig(  int const& d,     // Datapoint index
                      int const& ix1,   // First x-index
                      int const& ix2,   // Second x-index
                      int const& ifl1,  // First flavour index
                      int const& ifl2   // Second flavour index
                    ) const;

        // DIS version of GetISig
        int GetISig(  int const& d,     // Datapoint index
                      int const& ix,    // x-index
                      int const& ifl    // flavour index
                   ) const;

        // Metadata
        const std::string fDataName;
        const std::string fDescription;
        const int   fNData;

        // Process information
        const double  fQ20;
        const bool  fHadronic;
        const int   fNonZero;
        int *const  fFlmap;

        // x-grid information
        const int   fNx;
        const int   fTx;
        const int   fRmr;
        const int   fPad;
        const int   fDSz;

        // X-arrays        
        double *const fXgrid;

        // FK table
        T *const fSigma;

        // Cfactor information
        const bool fHasCFactors;
        double *const fcFactors;

      private:
        FKTable();                          //!< Disable default constructor
        FKTable& operator=(const FKTable&); //!< Disable copy-assignment

        void InitialiseFromStream(std::istream&, std::vector<std::string> const& cFactors); //!< Initialise the FK table from an input stream
        void CachePDF(extern_pdf inpdf, size_t const& NPDF, T* pdf); // Cache PDF for convolution

        int parseNonZero(); // Parse flavourmap information into fNonZero
    };

 // **********************************************************************************


#ifdef HAVE_SSE3
  #include <pmmintrin.h>

  static inline void convolute(const float* __restrict__ x, const float* __restrict__ y, float& retval, int const& n)
  {
    __m128 acc = _mm_setzero_ps();

    const __m128* a = (const __m128*) x;
    const __m128* b = (const __m128*) y;

    for (int i=0; i<n/4; i++)
      acc = _mm_add_ps(acc, _mm_mul_ps(*(a+i),*(b+i)));

    const __m128 shuffle1 = _mm_shuffle_ps(acc, acc, _MM_SHUFFLE(1,0,3,2));
    const __m128 tmp1     = _mm_add_ps(acc, shuffle1);
    const __m128 shuffle2 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2,3,0,1));
    const __m128 tmp2     = _mm_add_ps(tmp1, shuffle2);

    _mm_store_ss(&retval,tmp2);
    _mm_empty();

    return;
  }
#else
  // Basic single-precision convolution
  static inline void convolute(const float* __restrict__ pdf, const float* __restrict__ sig, float& retval, int const& n)
  {
    for (int i = 0; i < n; i++)
      retval += pdf[i]*sig[i];
  }
#endif

  // Basic double-precision convolution
  static inline void convolute(const double* __restrict__ pdf, const double* __restrict__ sig, double& retval, int const& n)
  {
    for (int i = 0; i < n; i++)
      retval += pdf[i]*sig[i];
  }

 // **********************************************************************************


  /**
   * @brief Constructor for FK Table
   * @param filename The FK table filename
   * @param cFactors A vector of filenames for potential C-factors
   */
  template<typename T>
  FKTable<T>::FKTable( std::string const& filename, 
                    std::vector<std::string> const& cFactors):
  FKHeader(filename),
  fDataName(    GetTag        (GRIDINFO,       "SETNAME")),
  fDescription( GetTag        (BLOB,   "GridDesc")),
  fNData(       GetTag<int>   (GRIDINFO,   "NDATA")),
  fQ20(std::pow(GetTag<double>(THEORYINFO, "Q0"),2)),
  fHadronic(    GetTag<bool>  (GRIDINFO,   "HADRONIC")),
  fNonZero(parseNonZero()),  // All flavours
  fFlmap(fHadronic ? new int[2*fNonZero]:new int[fNonZero]),
  fNx(          GetTag<int>   (GRIDINFO,   "NX")),
  fTx(fHadronic ? fNx*fNx:fNx),
  fRmr(fTx*fNonZero % convoluteAlign<T>()),
  fPad((fRmr == 0) ? 0:convoluteAlign<T>() - fRmr ),
  fDSz( fTx*fNonZero + fPad ),
  fXgrid(new double[fNx]),
  fSigma( new T[fDSz*fNData]),
  fHasCFactors(cFactors.size()),
  fcFactors(new double[fNData])
  {
    std::ifstream is(filename);
    NNPDF::FKHeader headSkip(is);
    InitialiseFromStream(is, cFactors);
  };

  /**
   * @brief Constructor for FK Table
   * @param is An input stream for reading from
   * @param cFactors A vector of filenames for potential C-factors
   */
  template<typename T>
  FKTable<T>::FKTable( std::istream& is, std::vector<std::string> const& cFactors ):
  FKHeader(is),
  fDataName(    GetTag        (GRIDINFO,       "SETNAME")),
  fDescription( GetTag        (BLOB,   "GridDesc")),
  fNData(       GetTag<int>   (GRIDINFO,   "NDATA")),
  fQ20(     pow(GetTag<double>(THEORYINFO, "Q0"),2)),
  fHadronic(    GetTag<bool>  (GRIDINFO,   "HADRONIC")),
  fNonZero(parseNonZero()),  // All flavours
  fFlmap(fHadronic ? new int[2*fNonZero]:new int[fNonZero]),
  fNx(          GetTag<int>   (GRIDINFO,   "NX")),
  fTx(fHadronic ? fNx*fNx:fNx),
  fRmr(fTx*fNonZero % convoluteAlign<T>()),
  fPad((fRmr == 0) ? 0:convoluteAlign<T>() - fRmr ),
  fDSz( fTx*fNonZero + fPad ),
  fXgrid(new double[fNx]),
  fSigma( new T[fDSz*fNData]),
  fHasCFactors(cFactors.size()),
  fcFactors(new double[fNData])
  {
    InitialiseFromStream(is, cFactors);
  };

  /**
   * @brief FKTable copy-constructor.
   * @param set  The FK table to be copied
   */
  template<typename T>
  FKTable<T>::FKTable(FKTable const& set):
  FKHeader(set),
  fDataName(set.fDataName),
  fNData(set.fNData),
  fQ20(set.fQ20),
  fHadronic(set.fHadronic),
  fNonZero(set.fNonZero),
  fFlmap(fHadronic ? (new int[2*fNonZero]):(new int[fNonZero])),
  fNx(set.fNx),
  fTx(set.fTx),
  fRmr(set.fRmr),
  fPad(set.fPad),
  fDSz(set.fDSz),
  fXgrid(new double[fNx]),
  fSigma(new T[fDSz*fNData]),
  fHasCFactors(set.fHasCFactors),
  fcFactors(new double[fNData])
  {
     // Copy X grid
    for (int i = 0; i < fNx; i++)
      fXgrid[i] = set.fXgrid[i];
      
    // Copy flavour map
    if (IsHadronic()) // Hadronic
    {
      for (int fl = 0; fl < fNonZero; fl++)
      {
        fFlmap[2*fl] = set.fFlmap[2*fl];
        fFlmap[2*fl+1] = set.fFlmap[2*fl+1];
      }
    }
    else  // DIS
    {
      for (int fl = 0; fl < fNonZero; fl++)
        fFlmap[fl] = set.fFlmap[fl];
    }
    
    // Copy reduced FK table
    for (int i = 0; i < fNData; i++)
      {
        for (int j = 0; j < fDSz; j++)
          fSigma[i*fDSz + j] = set.fSigma[i*fDSz + j];
        fcFactors[i] = set.GetCFactors()[i];
      }
  }

  /**
   * @brief Masked FK Table copy constructor.
   * @param set  The FK table to be filtered
   * @param mask The mask from filter
   */
  template<typename T>
  FKTable<T>::FKTable(FKTable const& set, std::vector<int> const& mask):
  FKHeader(set),
  fDataName(set.fDataName),
  fNData(mask.size()),
  fQ20(set.fQ20),
  fHadronic(set.fHadronic),
  fNonZero(set.fNonZero),
  fFlmap(fHadronic ? (new int[2*fNonZero]):(new int[fNonZero])),
  fNx(set.fNx),
  fTx(set.fTx),
  fRmr(set.fRmr),
  fPad(set.fPad),
  fDSz(set.fDSz),
  fXgrid(new double[fNx]),
  fSigma(new T[fDSz*fNData]),
  fHasCFactors(set.fHasCFactors),
  fcFactors(new double[fNData])
  {
     if (fNData == 0)
       throw RangeError("FKTable::FKTable", "datapoints cut to 0!");

    // Copy X grid
    for (int i = 0; i < fNx; i++)
      fXgrid[i] = set.fXgrid[i];
      
    // Copy flavour map
    if (IsHadronic()) // Hadronic
    {
      for (int fl = 0; fl < fNonZero; fl++)
      {
        fFlmap[2*fl] = set.fFlmap[2*fl];
        fFlmap[2*fl+1] = set.fFlmap[2*fl+1];
      }
    } else  // DIS
    {
      for (int fl = 0; fl < fNonZero; fl++)
        fFlmap[fl] = set.fFlmap[fl];
    }
    
    // Copy reduced FK table
    for (int i = 0; i < fNData; i++)
      {
        for (int j = 0; j < fDSz; j++)
          fSigma[i*fDSz + j] = set.fSigma[mask[i]*fDSz + j];
        fcFactors[i] = set.GetCFactors()[mask[i]];
      }
  }

  /**
   * @brief FKTable destructor
   */
  template<typename T>
  FKTable<T>::~FKTable()
  {
    delete[] fSigma;
    delete[] fFlmap;
    delete[] fXgrid;
    delete[] fcFactors;
  }

  /**
   * @brief Method for initialisation from stream
   * @param is the input stream after reading the FK header
   */
  template<typename T>
  void FKTable<T>::InitialiseFromStream( std::istream& is, std::vector<std::string> const& cFactors )
  {
   if (Verbose)
    {
     std::cout <<SectionHeader(fDataName.c_str(),GRIDINFO)<<std::endl;
     std::cout << fDescription << std::endl;
    }

    // Read FlavourMap from header
    const int nFL = 14;
    std::stringstream fmBlob(GetTag(BLOB,"FlavourMap"));

    if (!fHadronic) // DIS flavourmap
    {
      bool maxMap[nFL]; // Maximal size flavourmap
      for (int i=0; i<nFL; i++)
        fmBlob >> maxMap[i];
     
      // Build FLMap
      int index = 0;
      for (int i=0; i<nFL; i++)
        if (maxMap[i]) { fFlmap[index] = i; index++; }
    }
    else // Hadronic flavourmap
    {
      bool maxMap[nFL][nFL]; // Maximal size flavourmap
      for (int i=0; i<nFL; i++)
        for (int j=0; j<nFL; j++)
          fmBlob >> maxMap[i][j];

      // Build FLMap
      int index = 0;
      for (int i=0; i<nFL; i++)
        for (int j=0; j<nFL; j++)
          if (maxMap[i][j])
          {
            fFlmap[2*index] = i;
            fFlmap[2*index+1] = j;
            index++;
          }
    }

    // Read x grid - need more detailed checks
    std::stringstream xBlob(GetTag(BLOB,"xGrid"));
    for (int i=0; i<fNx; i++)
      xBlob >> fXgrid[i];

    // Sanity checks
    if ( fNData <= 0 )
      throw RangeError("FKTable::FKTable","Number of datapoints is set to: " + std::to_string(fNData) );
    
    if ( fNx <= 0 )
      throw RangeError("FKTable::FKTable","Number of x-points is set to: " + std::to_string(fNx) );
    
    if ( fNonZero <= 0 )
      throw RangeError("FKTable::FKTable","Number of nonzero flavours is set to: " + std::to_string(fNonZero) );

    if (Verbose)
      std::cout << fNData << " Data Points "
      << fNx << " X points "
      << fNonZero << " active flavours " << std::endl;


    // Zero sigma array -> also zeros pad quantities
    for (int i=0; i<fDSz*fNData; i++)
      fSigma[i]=0;

    // Read Cfactors
    for (int i = 0; i < fNData; i++) fcFactors[i] = 1.0;
      for (size_t i=0; i<cFactors.size(); i++)
       ReadCFactors(cFactors[i]);

    // Read FastKernel Table
    std::string line;
    std::vector<T> datasplit;

    if (fHadronic) {
      while (getline(is,line))
      {
        split(datasplit,line);
        const int d = datasplit[0]; // datapoint
        const int a = datasplit[1]; // x1 index
        const int b = datasplit[2]; // x2 index
              
        for (int j=0; j<fNonZero; j++)
          {
            const int targetFlIndex = 14*fFlmap[2*j] + fFlmap[2*j+1]+3;
            fSigma[ d*fDSz+j*fTx+a*fNx+b ] = fcFactors[d]*datasplit[targetFlIndex];
          }
      }
    } else { // DIS
      while (getline(is,line))
      {
        split(datasplit,line);
        
        const int d = datasplit[0];
        const int a = datasplit[1];
        
        for (int j=0; j<fNonZero; j++)
          fSigma[ d*fDSz+j*fNx+a ] = fcFactors[d]*datasplit[fFlmap[j]+2];
      }
    }  
  }



  /**
   * @brief FKTable print to ostream
   */
  template<typename T>
  void FKTable<T>::Print(std::ostream& os)
  {
    if (Verbose)
      std::cout << "****** Exporting FKTable: "<<fDataName << " ******"<< std::endl;

    // Verify current flavourmap
    std::string nflmap;
    std::string cflmap = GetTag(BLOB, "FlavourMap");
    const bool optmap = OptimalFlavourmap(nflmap);
    if (!optmap)
    {
      RemTag(BLOB, "FlavourMap");
      AddTag(BLOB, "FlavourMap", nflmap);
    }

    // Print header
    FKHeader::Print(os);

    // Check that stream is still good
    if (!os.good())
      throw FileError("FKTable::Print","no good outstream!");

    if (fHasCFactors != 0)
    {
      std::cout << "FKTable::Print Warning: EXPORTING AN FKTABLE COMBINED WITH C-FACTORS" << std::endl;
      std::cout << "                        PLEASE ENSURE THAT THIS IS INTENTIONAL!" << std::endl;
    }

    // Write FastKernel Table
    if (fHadronic) 
    {
      for (int d=0; d<fNData; d++)
        for(int a=0; a<fNx; a++ )
          for (int b=0; b<fNx; b++)
          {
            bool isNonZero = false;
            std::stringstream outputline;
            for (int i=0; i<14; i++)
              for (int j=0; j<14; j++)
              {
                const int iSigma = GetISig(d,a,b,i,j);

                // Set precision
                if (iSigma == -1)
                  outputline << std::fixed <<std::setprecision(0);
                else if (fSigma[iSigma] == 0.0)
                  outputline << std::fixed <<std::setprecision(0);
                else
                  outputline << std::scientific << std::setprecision(16);

                outputline << (iSigma == -1 ? 0:fSigma[iSigma]) << "\t";

                if (iSigma != -1) if (fSigma[iSigma] != 0) isNonZero = true;
              }

            if (isNonZero) 
              os << d << "\t" << a <<"\t" << b <<"\t" <<  outputline.str() <<std::endl;
          }
    } else { // DIS
      for (int d=0; d<fNData; d++)
        for(int a=0; a<fNx; a++ )
          {
            bool isNonZero = false;
            std::stringstream outputline;
            for (int i=0; i<14; i++)
              {
                const int iSigma = GetISig(d,a,i);

                // Set precision
                if (iSigma == -1)
                  outputline << std::fixed <<std::setprecision(0);
                else if (fSigma[iSigma] == 0.0)
                  outputline << std::fixed <<std::setprecision(0);
                else
                  outputline << std::scientific << std::setprecision(16);

                outputline << (iSigma == -1 ? 0:fSigma[iSigma]) << "\t";
                if (iSigma != -1) if (fSigma[iSigma] != 0) isNonZero = true;
              }

            if (isNonZero)
              os << d << "\t" << a <<"\t" << outputline.str() <<std::endl;
          }
    }      

    // Restore current map
    if (!optmap)
    {
      RemTag(BLOB, "FlavourMap");
      AddTag(BLOB, "FlavourMap", cflmap);
    }
  
    return;    
  }

  template<typename T>
  void FKTable<T>::ReadCFactors(std::string const& cfilename)
  {
    std::fstream g;
    g.open(cfilename.c_str(), std::ios::in);
    if (g.fail())
      throw FileError("FKTable::FKTable","cannot open cfactor file: " + cfilename);

    // Read through header
    std::string line;
    int nDelin = 0;
    while (nDelin < 2)
    {
      const int peekval = (g >> std::ws).peek();
      if (peekval == FK_DELIN_KEY)
        nDelin++;
      getline(g,line);
    }

    // Read C-factors
    getline(g,line); T tmp;
    for (int i = 0; i < fNData; i++)
    {
      g >> tmp;
      fcFactors[i] *= tmp;
    }

    g.close();
  }

  // Perform convolution
  template<typename T>
  void FKTable<T>::Convolute(extern_pdf inpdf, size_t const& Npdf, T* out)
  {
    // Fetch PDF array
    T *pdf = 0;
    int err = posix_memalign(reinterpret_cast<void **>(&pdf), 16, sizeof(T)*fDSz*Npdf);
    if (err != 0)
      throw RangeError("FKTable::Convolute","ThPredictions posix_memalign " + std::to_string(err));
    CachePDF(inpdf, Npdf, pdf);

    // Calculate observables
    //#pragma omp parallel for
    for (int i = 0; i < fNData; i++)
      for (size_t n = 0; n < Npdf; n++)
      {
        out[i*Npdf + n] = 0;
        convolute(pdf+fDSz*n,fSigma+fDSz*i,out[i*Npdf + n],fDSz);
      }

    // Delete pdfs
    free(reinterpret_cast<void *>(pdf));
    return;
  }

  // Perform convolution
  template<typename T>
  void FKTable<T>::CachePDF(extern_pdf inpdf, size_t const& NPDF, T* pdf)
  {
    // prepare PDF representation
    int index = 0;
    const int NFL = 14;
    T* EVLN = new T[fNx*NFL];
    
    for (size_t n = 0; n < NPDF; n++)
    {
      for (int i = 0; i < fNx; i++)
        inpdf(fXgrid[i], sqrt(fQ20), n, &EVLN[i*NFL]);
      
      for (int fl=0; fl<fNonZero; fl++)
      {
        const int fl1 = fFlmap[2*fl];
        const int fl2 = fFlmap[2*fl+1];
        
        for (int i = 0; i < fNx; i++)
          for (int j = 0; j < fNx; j++)
            pdf[index++] = EVLN[i*NFL+fl1]*EVLN[j*NFL+fl2];
      }
      
      for (int i=0; i<fPad; i++) // Add padding PDF
        pdf[index++] = 0;
      
    }
    
    delete[] EVLN;
    return;
  }


  // GetFKValue returns the appropriate point of the FK table
  template<typename T>
  int FKTable<T>::GetISig   (   int const& d,     // Datapoint index
                                int const& a,   // First x-index
                                int const& b,   // Second x-index
                                int const& ifl1,  // First flavour index
                                int const& ifl2   // Second flavour index
                            ) const
  {

    if (!fHadronic)
      throw EvaluationError("FKTable::GetISig","Hadronic call for DIS table!");

    // Identify which nonZero flavour ifl1 and ifl2 correspond to
    int j = -1;
    for (int i=0; i < fNonZero; i++)
      if ( ifl1 == fFlmap[2*i] && ifl2 == fFlmap[2*i+1])
      {
        j = i;
        break;
      }

    // Not in FLmap
    if (j == -1)
      return -1;

    // Not in dataset
    if (d >= fNData)
      return -1;

    // Not in x-grid
    if (a >= fNx || b >= fNx)
      return -1;

    // Return pointer to FKTable segment
    return d*fDSz+j*fTx+a*fNx+b ;
  }

  // DIS version of GetFKValue
  template<typename T>
  int FKTable<T>::GetISig(  int const& d,     // Datapoint index
                         int const& a,    // x-index
                         int const& ifl    // flavour index
                       ) const
  {

    if (fHadronic)
      throw EvaluationError("FKTable::GetISig","DIS call for Hadronic table!");

    // Identify which nonZero flavour ifl corresponds to
    int j = -1;
    for (int i=0; i < fNonZero; i++)
      if ( ifl == fFlmap[i] )
      {
        j = i;
        break;
      }

    // Not in FLmap
    if (j == -1)
      return -1;

    // Not in dataset
    if (d >= fNData)
      return -1;

    // Not in x-grid
    if (a >= fNx)
      return -1;

    return d*fDSz+j*fNx+a;
  }

  template<typename T>
  bool FKTable<T>::OptimalFlavourmap(std::string& flmap) const  //!< Determine and return the optimal flavour map
  {
      bool nonZero[fNonZero];
      for (int j=0; j < fNonZero; j++)
      {
        nonZero[j] = false;
        for (int d=0; d < fNData; d++)
        {
          if (nonZero[j]) break;

          for (int a=0; a<fTx; a++)
            if (fSigma[d*fDSz+j*fTx+a] != 0)
            {
              nonZero[j] = true;
              break;
            }
        }
      }

      std::stringstream outmap;
      if (fHadronic)
      {
        for (int i=0; i<14; i++)
        {
          for (int j=0; j<14; j++)
          {
            bool found = false;
            for (int k=0; k<fNonZero; k++)
              if (nonZero[k] == true)
                if (fFlmap[2*k] == i && fFlmap[2*k+1] == j)
                  found = true;
            if (found) outmap << 1 <<" ";
            else outmap << 0 <<" ";
          }
          outmap << std::endl;
        }
      }
      else
      {
        for (int i=0; i<14; i++)
        {
          bool found = false;
          for (int k=0; k<fNonZero; k++)
            if (nonZero[k] == true)
              if (fFlmap[k] == i)
                found = true;
          if (found) outmap << 1 <<" ";
          else outmap << 0 <<" ";
        }
        
        outmap << std::endl;
      }

    flmap = outmap.str();

    bool anyZeros = false;
    for (int i=0; i<fNonZero; i++)
      if (nonZero[i] == false)
        anyZeros = true;

    return !anyZeros;
  }

  template<typename T>
  int FKTable<T>::parseNonZero()
  {
    // Fetch the flavourmap blob
    std::stringstream fmBlob(GetTag(BLOB,"FlavourMap"));
    int nNonZero = 0;

    for (int i=0; i<14; i++)
    {

      if (!fmBlob.good())
        throw RangeError("FKTable::parseNonZero","FlavourMap formatting error!");

      if (!fHadronic)
      {
          bool iNonZero = false; fmBlob >> iNonZero;
          nNonZero += iNonZero;
      }
      else
      {
        for (int j=0; j<14; j++)
          if (fmBlob.good())
          {
            bool iNonZero = false; fmBlob >> iNonZero;
            nNonZero += iNonZero;
          }
          else
            throw RangeError("FKTable::parseNonZero","FlavourMap formatting error!");
      }
    }

    return nNonZero;
  }


}