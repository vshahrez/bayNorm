#include <RcppArmadillo.h>
#include <RcppArmadilloExtensions/sample.h>
#include <Rmath.h>
#include <Rcpp.h>
// [[Rcpp::depends(RcppArmadillo)]]

using namespace Rcpp ;
using namespace arma;
using namespace std;


struct add_multiple {
  int incr;
  int count;
  add_multiple(int incr)
    : incr(incr), count(0)
  {}
  inline int operator()(int d) {
    return d + incr * count++;
  }
};

Rcpp::NumericVector rcpp_seq(double from_, double to_, double by_ = 1.0) {
  int adjust = std::pow(10, std::ceil(std::log10(10 / by_)) - 1);
  int from = adjust * from_;
  int to = adjust * to_;
  int by = adjust * by_;

  std::size_t n = ((to - from) / by) + 1;
  Rcpp::IntegerVector res = Rcpp::rep(from, n);
  add_multiple ftor(by);

  std::transform(res.begin(), res.end(), res.begin(), ftor);
  return Rcpp::NumericVector(res) / adjust;
}


double chooseC(double n, double k) {
  return Rf_choose(n, k);
}




double compute_prob2(double n, double m, double beta) {
  double prob;
  arma::vec k = arma::linspace<vec>(0, m-1, m);
  arma::vec k_vec;

  if(n<m){prob=0;}
  else{
    if(chooseC(n,m)==R_PosInf){

      k_vec= (n-k)/(m-k)*pow((1-beta),(n-m)/m)*beta;

      prob=arma::prod(k_vec)*beta;
    }
    else{
      prob = beta * chooseC(n,m) * pow(beta,m) * pow((1-beta),(n-m));
    }

  }
  return(prob);
}




double compute_prob_prior2(double n, double m,  double beta, double size, double m_ave) {
  double prob;
  arma::vec k = arma::linspace<vec>(0, m-1, m);
  arma::vec k_vec;

  if(n<m){prob=0;}
  else{

    if(chooseC(n,m)==R_PosInf){

      k_vec= (n-k)/(m-k)*pow((1-beta),(n-m)/m)*beta;

      prob=prod(k_vec)*beta*dnbinom_mu(n,size,m_ave,0);
    }

    else{
      prob = beta *chooseC(n,m) * pow(beta,m) * pow((1-beta),(n-m))*dnbinom_mu(n, size, m_ave, 0);
    }

  }
  return(prob);

}





NumericVector Compute2function(IntegerVector x,double m,double beta,double size, double m_ave, int last,int Indicate) {


  NumericVector y(last+1);

  if(Indicate==1)
  {
    for(int temp=0;temp<(last+1);temp++){
      y(temp)=compute_prob_prior2(x(temp), m, beta, size, m_ave);
    }
  }

  else{
    for(int temp2=0;temp2<(last+1);temp2++){
      y(temp2)=compute_prob2(x(temp2), m, beta);
    }
  }
  return(y);
}


NumericVector Compute2function_norm(IntegerVector x,double m,double beta,int last, int init) {

  NumericVector y(last-init+1);

    for(int temp2=0;temp2<(last-init+1);temp2++){
      y(temp2)=R::dnorm(m,x(temp2)*beta,x(temp2)*beta*(1- beta),false);
    }
  return(y);
}


//' Main_Bay
//'
//' bayNorm
//' there are leading NA, marking the leadings NA as TRUE and
//' everything else as FALSE.
//'
//' @param Table: raw count table
//' @param Beta_origin: A vector of capture efficiencies of cells
//' @param size: A vector of size
//' @param M_ave_ori: A vector of mu
//' @param S: number of samples that you want to generate
//' @param thres:thres
//' @param Mean_depth:Mean_depth
//' @return bayNorm
//' @export
// [[Rcpp::export]]
NumericVector Main_Bay(NumericMatrix Table, NumericVector Beta_origin, NumericVector size, Nullable<NumericVector> M_ave_ori,int S,int thres,double Mean_depth, LogicalVector debug=false)
{



  arma::mat M = Rcpp::as<arma::mat>(Table);

  arma::vec Beta = Rcpp::as<arma::vec>(Beta_origin);
  arma::vec M_ave;
  arma::mat M_t;

  int nrow=M.n_rows;
  int ncol=M.n_cols;
  int i;
  int j;
  int q;
  int dim;

  int last;
  int init;
  int NormalApproThre=500;

  IntegerVector x;
  NumericVector y;

  arma::cube Final_mat(nrow, ncol, S);


  if (M_ave_ori.isNotNull())
  {

    M_ave = Rcpp::as<arma::vec>(M_ave_ori);
  }


  else{

    arma::rowvec M_colmean=arma::sum(M, dim=0 );

    M_t=M.each_row() / M_colmean;
    M_ave = arma::mean(M_t, dim=1 )*Mean_depth;
  }


  for( i=0;i<ncol;i++){
    Rcout << "The cell is \n" << i+1 << std::endl;

    for( j=0;j<nrow;j++){

      //if(debug)
      //{Rcout << "The gene is \n" << j+1 << std::endl;}

      if(M(j,i)==NA_INTEGER) {
        for( q=0;q<S;q++){Final_mat(j,i,q)=NA_INTEGER;}
      }

      else{



        if(M(j,i)<NormalApproThre){
      last=floor((M(j,i)+1)*3/Beta(i));
      x=(seq_len(last+1)-1);

        if(M(j,i)<thres){

          y=Compute2function(x,M(j,i),Beta(i),size(j), M_ave(j), last,1);
        }

        else{
          y=Compute2function(x,M(j,i),Beta(i),size(j), M_ave(j), last,0);
        }
        }else{
          init=M(j,i)/3/Beta(i);
          last=M(j,i)*3/Beta(i);
          x=rcpp_seq(init,last,1);
           y=Compute2function_norm(x,M(j,i),Beta(i),last,init);

        }

        NumericVector z=y/sum(y);


        //Rcout << "The gene is \n" << j+1 << std::endl;
        IntegerVector S_temp=Rcpp::RcppArmadillo::sample(x, S, true, z);
        arma::vec S_input = Rcpp::as<arma::vec>(S_temp);
        Final_mat.subcube(j,i,0,j,i,S-1)=S_input;


      } //end of else


    } //j

  }  //i


  NumericVector Final_mat2=Rcpp::wrap(Final_mat);
  Rcpp::rownames(Final_mat2) = Rcpp::rownames(Table);
  Rcpp::colnames(Final_mat2) = Rcpp::colnames(Table);

return(Rcpp::wrap(Final_mat2));
}


//' Mode_Bay
//'
//' bayNorm
//' there are leading NA, marking the leadings NA as TRUE and
//' everything else as FALSE.
//'
//' @param Table: raw count table
//' @param Beta_origin: A vector of capture efficiencies of cells
//' @param size: A vector of size
//' @param M_ave_ori: A vector of mu
//' @param S: number of samples that you want to generate
//' @param thres:thres
//' @param Mean_depth:Mean_depth
//' @return bayNorm
//' @export
// [[Rcpp::export]]
NumericMatrix Main_mode_Bay(NumericMatrix Table, NumericVector Beta_origin, NumericVector size, Nullable<NumericVector> M_ave_ori,int S,int thres,double Mean_depth)
{



  arma::mat M = Rcpp::as<arma::mat>(Table);

  arma::vec Beta = Rcpp::as<arma::vec>(Beta_origin);
  arma::vec M_ave;
  arma::mat M_t;

  int nrow=M.n_rows;
  int ncol=M.n_cols;
  int i;
  int j;
  int q;
  int dim;
  int NormalApproThre=500;

  int last;
  int init;
  IntegerVector x;
  NumericVector y;

  arma::mat Final_mat(nrow, ncol);


  if (M_ave_ori.isNotNull())
  {

    M_ave = Rcpp::as<arma::vec>(M_ave_ori);
  }


  else{
    arma::rowvec M_colmean=arma::sum(M, dim=0 );
    M_t=M.each_row() / M_colmean;
    M_ave = arma::mean(M_t, dim=1 )*Mean_depth;
  }


  for( i=0;i<ncol;i++){
    Rcout << "The cell is \n" << i+1 << std::endl;

    for( j=0;j<nrow;j++){

      if(M(j,i)==NA_INTEGER) {
        for( q=0;q<S;q++){Final_mat(j,i)=NA_INTEGER;}
      }

      else{

          if(M(j,i)<NormalApproThre){

          last=floor((M(j,i)+1)*3/Beta(i));
          x=(seq_len(last+1)-1);

          if(M(j,i)<thres){

            y=Compute2function(x,M(j,i),Beta(i),size(j), M_ave(j), last,1);
          }

          else{
            y=Compute2function(x,M(j,i),Beta(i),size(j), M_ave(j), last,0);
          }
        }else{
          init=M(j,i)/3/Beta(i);
          last=M(j,i)*3/Beta(i);
          x=rcpp_seq(init,last,1);
          y=Compute2function_norm(x,M(j,i),Beta(i),last,init);

        }

        NumericVector z=y/sum(y);

        arma::vec x_arma= Rcpp::as<arma::vec>(x);
        arma::vec z_arma= Rcpp::as<arma::vec>(z);
        arma::uword max_index=index_max(z_arma);
        Final_mat(j,i)=mean(x_arma(max_index));

        //Rcout << "The gene is \n" << j+1 << std::endl;
        //IntegerVector S_temp=Rcpp::RcppArmadillo::sample(x, S, true, z);
        //arma::vec S_input = Rcpp::as<arma::vec>(S_temp);

        //Final_mat.subcube(j,i,0,j,i,S-1)=S_input;

      } //end of else


    } //j

  }  //i



  NumericVector Final_mat2=Rcpp::wrap(Final_mat);
  Rcpp::rownames(Final_mat2) = Rcpp::rownames(Table);
  Rcpp::colnames(Final_mat2) = Rcpp::colnames(Table);

  return(Rcpp::wrap(Final_mat2));
}






double D_SIZE(double SIZE,double MU, NumericVector m_observed,NumericVector BETA) {

  NumericVector m=m_observed;
  int nCells=m.size();

  NumericVector temp_vec_2(nCells);


  double MarginalVal;

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;
    int n_length=n.size();
    NumericVector numerator_1(n_length);
    NumericVector numerator_2(n_length);
    NumericVector kernel_vec(n_length);

    for(int j=0;j<n_length;j++){

      kernel_vec(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE,MU,false);
      numerator_1(j)=R::digamma(n(j)+SIZE)-R::digamma(SIZE)+log(SIZE/(SIZE+MU))+(MU-n(j))/(MU+SIZE);
      numerator_2(j)=kernel_vec(j)*numerator_1(j);
    }


    temp_vec_2(i)=sum(numerator_2)/sum(kernel_vec);


  }

  MarginalVal=sum(temp_vec_2);
  return MarginalVal;
}




double D_MU(double SIZE,double MU, NumericVector m_observed,NumericVector BETA) {
  NumericVector m=m_observed;
  int nCells=m.size();
  NumericVector temp_vec_2(nCells);
  double MarginalVal;

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;
    int n_length=n.size();
    NumericVector numerator_1(n_length);
    NumericVector numerator_2(n_length);
    NumericVector kernel_vec(n_length);

    for(int j=0;j<n_length;j++){

      kernel_vec(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE,MU,false);
      numerator_1(j)=SIZE*(n(j)-MU)/(MU*(MU+SIZE));
      numerator_2(j)=kernel_vec(j)*numerator_1(j);

    }
    temp_vec_2(i)=sum(numerator_2);
  }
  MarginalVal=sum(temp_vec_2);
  return MarginalVal;
}





NumericVector D_SIZE_MU_2D(NumericVector SIZE_MU, NumericVector m_observed,NumericVector BETA) {

  NumericVector m=m_observed;
  int nCells=m.size();
  NumericVector temp_vec_2_SIZE(nCells);
  NumericVector temp_vec_2_MU(nCells);

  NumericVector MarginalVal_vec(2);

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;

    int n_length=n.size();
    NumericVector numerator_SIZE(n_length);
    NumericVector numerator_MU(n_length);
    NumericVector numerator_2_SIZE(n_length);
    NumericVector numerator_2_MU(n_length);
    NumericVector kernel_vec(n_length);

    for(int j=0;j<n_length;j++){

      kernel_vec(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE_MU(0),SIZE_MU(1),false);
      numerator_SIZE(j)=R::digamma(n(j)+SIZE_MU(0))-R::digamma(SIZE_MU(0))+log(SIZE_MU(0)/(SIZE_MU(0)+SIZE_MU(1)))+(SIZE_MU(1)-n(j))/(SIZE_MU(1)+SIZE_MU(0));
      numerator_MU(j)=SIZE_MU(0)*(n(j)-SIZE_MU(1))/(SIZE_MU(1)*(SIZE_MU(1)+SIZE_MU(0)));
      numerator_2_SIZE(j)=kernel_vec(j)*numerator_SIZE(j);
      numerator_2_MU(j)=kernel_vec(j)*numerator_MU(j);

    }

    temp_vec_2_SIZE(i)=sum(numerator_2_SIZE)/sum(kernel_vec);
    temp_vec_2_MU(i)=sum(numerator_2_MU)/sum(kernel_vec);
  }
  MarginalVal_vec[0]=sum(temp_vec_2_SIZE);
  MarginalVal_vec[1]=sum(temp_vec_2_MU);
  return MarginalVal_vec;
}



//' GradientFun_2D
//'
//' GradientFun_2D
//'
//' @param SIZE_MU:SIZE_MU
//' @param m_observed: m_observed
//' @param BETA: BETA
//' @return GradientFun_2D
//' @export
// [[Rcpp::export]]
NumericVector GradientFun_2D(NumericVector SIZE_MU, NumericVector m_observed,NumericVector BETA){
  NumericVector m=m_observed;
  NumericVector Grad_vec(2);
  Grad_vec=D_SIZE_MU_2D(SIZE_MU,m,BETA);
  return Grad_vec;
}



//' MarginalF_2D
//'
//' MarginalF_2D
//'
//' @param SIZE:SIZE
//' @param MU:MU
//' @param size: A vector of size
//' @param M_ave_ori: A vector of mu
//' @param m_observed:m_observed
//' @param BETA:BETA
//' @return Marginal likelihood
//' @export
// [[Rcpp::export]]
double MarginalF_2D(NumericVector SIZE_MU, NumericVector m_observed, NumericVector BETA) {
  NumericVector m=m_observed;
  int nCells=m.size();
  NumericVector temp_vec_2(nCells);


  double MarginalVal;

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;
    int n_length=n.size();
    NumericVector temp_vec_1(n_length);

    for(int j=0;j<n_length;j++){

      temp_vec_1(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE_MU(0),SIZE_MU(1),false);

    }

    arma::vec temp_vec_1arma = Rcpp::as<arma::vec>(temp_vec_1);

    temp_vec_2(i)=sum(temp_vec_1arma);

  }
  MarginalVal=sum(log(temp_vec_2));
  return MarginalVal;
}


//1D optimization



double D_SIZE_MU_1D(double SIZE,double MU, NumericVector m_observed,NumericVector BETA) {

  NumericVector m=m_observed;
  int nCells=m.size();
  NumericVector temp_vec_2_SIZE(nCells);
  NumericVector temp_vec_2_MU(nCells);

  double MarginalVal;

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;
    int n_length=n.size();
    NumericVector numerator_SIZE(n_length);
    NumericVector numerator_MU(n_length);
    NumericVector numerator_2_SIZE(n_length);
    NumericVector numerator_2_MU(n_length);
    NumericVector kernel_vec(n_length);

    for(int j=0;j<n_length;j++){

      kernel_vec(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE,MU,false);
      numerator_SIZE(j)=R::digamma(n(j)+SIZE)-R::digamma(SIZE)+log(SIZE/(SIZE+MU))+(MU-n(j))/(MU+SIZE);

      numerator_2_SIZE(j)=kernel_vec(j)*numerator_SIZE(j);
    }
    temp_vec_2_SIZE(i)=sum(numerator_2_SIZE)/sum(kernel_vec);
  }

  MarginalVal=sum(temp_vec_2_SIZE);
  return(MarginalVal);
}



//' GradientFun_1D
//'
//' GradientFun_1D
//'
//' @param SIZE:SIZE
//' @param MU:MU
//' @param m_observed: m_observed
//' @param BETA: BETA
//' @return GradientFun_1D
//' @export
// [[Rcpp::export]]
double GradientFun_1D(double SIZE,double MU, NumericVector m_observed,NumericVector BETA){
  NumericVector m=m_observed;
  double Gradd;
  Gradd=D_SIZE_MU_1D(SIZE, MU,m,BETA);
  return Gradd;

}



//' MarginalF_1D
//'
//' MarginalF_1D
//'
//' @param SIZE:SIZE
//' @param MU:MU
//' @param size: A vector of size
//' @param M_ave_ori: A vector of mu
//' @param m_observed:m_observed
//' @param BETA:BETA
//' @return Marginal likelihood
//' @export
// [[Rcpp::export]]
double MarginalF_1D(double SIZE,double MU, NumericVector m_observed, NumericVector BETA) {
  NumericVector m=m_observed;
  int nCells=m.size();
  NumericVector temp_vec_2(nCells);


  double MarginalVal;

  for(int i=0;i<nCells;i++){

    int last=floor((m(i)+1)*3/BETA(i));
    NumericVector n=rcpp_seq(0,last, 1.0) ;
    int n_length=n.size();
    NumericVector temp_vec_1(n_length);

    for(int j=0;j<n_length;j++){
      temp_vec_1(j)=R::dbinom(m(i),n(j),BETA(i),false)*R::dnbinom_mu(n(j),SIZE,MU,false);

    }

    arma::vec temp_vec_1arma = Rcpp::as<arma::vec>(temp_vec_1);

    temp_vec_2(i)=sum(temp_vec_1arma);

  }
  MarginalVal=sum(log(temp_vec_2));
  return MarginalVal;
}


