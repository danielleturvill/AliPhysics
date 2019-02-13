#include "TString.h"
#include "TMath.h"
#include "AliForwardFlowUtil.h"
#include "TFile.h"

#include <iostream>
#include <TROOT.h>
#include <TSystem.h>
#include <TInterpreter.h>
#include <TList.h>
#include <THn.h>

#include "AliLog.h"
#include "AliForwardFlowRun2Task.h"
#include "AliForwardQCumulantRun2.h"
#include "AliForwardGenericFramework.h"

#include "AliAODForwardMult.h"
#include "AliAODCentralMult.h"
#include "AliAODEvent.h"
#include "AliMCEvent.h"

#include "AliAODMCParticle.h"

#include "AliForwardFlowUtil.h"

#include "AliVVZERO.h"
#include "AliAODVertex.h"
#include "AliCentrality.h"

#include "AliESDEvent.h"
#include "AliVTrack.h"
#include "AliESDtrack.h"
#include "AliAODTrack.h"
#include "AliAODTracklets.h"

#include "AliAnalysisFilter.h"
#include "AliMultSelection.h"
#include "AliMultiplicity.h"
#include "AliAnalysisManager.h"
#include "AliInputEventHandler.h"

#include "AliStack.h"
#include "AliMCEvent.h"
#include "AliMCParticle.h"
//________________________________________________________________________
AliForwardFlowUtil::AliForwardFlowUtil():
fevent(),
fAODevent(),
fMCevent(),
mc(kFALSE),
dNdeta(),
fSettings(),
minpt(5),
maxpt(5),
dodNdeta(kTRUE)
{
}

void AliForwardFlowUtil::FillData(TH2D*& refDist, TH2D*& centralDist, TH2D*& forwardDist){
    if (!fSettings.mc) {
      AliAODEvent* aodevent = dynamic_cast<AliAODEvent*>(fevent);
      this->fAODevent = aodevent;
      AliAODForwardMult* aodfmult = static_cast<AliAODForwardMult*>(aodevent->FindListObject("Forward"));
      forwardDist = &aodfmult->GetHistogram();
      for (Int_t etaBin = 1; etaBin <= forwardDist->GetNbinsX(); etaBin++) {
        for (Int_t phiBin = 1; phiBin <= forwardDist->GetNbinsX(); phiBin++) {
          if (dodNdeta) dNdeta->Fill(forwardDist->GetXaxis()->GetBinCenter(etaBin),forwardDist->GetBinContent(etaBin, phiBin));
        }
      }

      if (fSettings.maxpt < 5) {
        this->minpt = 0.2;
        this->maxpt = 5;
        if (fSettings.useSPD) this->FillFromTracklets(refDist);
        else                  this->FillFromTracks(refDist, fSettings.tracktype);
      }
      this->minpt = fSettings.minpt;
      this->maxpt = fSettings.maxpt;
      if (fSettings.useSPD) this->FillFromTracklets(centralDist);
      else                  this->FillFromTracks(centralDist, fSettings.tracktype);
    }
    else {
      Float_t zvertex = this->GetZ();

      this->mc = kTRUE;

      if(!fMCevent)
        throw std::runtime_error("Not MC as expected");

      if (fSettings.esd){
        if (fSettings.use_primaries_cen && fSettings.use_primaries_fwd){
          this->FillFromPrimaries(centralDist, forwardDist);
        }
        else if (!fSettings.use_primaries_cen && !fSettings.use_primaries_fwd){
          this->FillFromTrackrefs(centralDist, forwardDist);
        }
        else if (fSettings.use_primaries_cen && !fSettings.use_primaries_fwd){
          this->FillFromPrimaries(centralDist);
          this->FillFromTrackrefs(forwardDist);
        }
        else if (!fSettings.use_primaries_cen && fSettings.use_primaries_fwd){
          this->FillFromTrackrefs(centralDist);
          this->FillFromPrimaries(forwardDist);
        }
      }
      else{ // AOD

        if (fSettings.use_primaries_cen && fSettings.use_primaries_fwd){ //prim central and forward
          if (fSettings.maxpt < 5.0) {
            this->minpt = 0.2;
            this->maxpt = 5.0;
            this->FillFromPrimariesAOD(refDist);
            this->minpt = fSettings.minpt;
            this->maxpt = fSettings.maxpt;
          }

          this->FillFromPrimariesAOD(centralDist, forwardDist);
        }
        else if (fSettings.use_primaries_cen && !fSettings.use_primaries_fwd){ //prim central, AOD forward
          if (fSettings.maxpt < 5.0) {
            this->minpt = 0.2;
            this->maxpt = 5.0;
            this->FillFromPrimariesAOD(refDist);
            this->minpt = fSettings.minpt;
            this->maxpt = fSettings.maxpt;
          }

          this->FillFromPrimariesAOD(centralDist);
          AliAODEvent* aodevent = dynamic_cast<AliAODEvent*>(fevent);

          AliAODForwardMult* aodfmult = static_cast<AliAODForwardMult*>(aodevent->FindListObject("Forward"));
          forwardDist = &aodfmult->GetHistogram();

          for (Int_t etaBin = 1; etaBin <= forwardDist->GetNbinsX(); etaBin++) {
            for (Int_t phiBin = 1; phiBin <= forwardDist->GetNbinsX(); phiBin++) {
              if (dodNdeta) dNdeta->Fill(forwardDist->GetXaxis()->GetBinCenter(etaBin),forwardDist->GetBinContent(etaBin, phiBin));
            }
          }
        }
      }
    }
    forwardDist->SetDirectory(0);
}


Bool_t AliForwardFlowUtil::ExtraEventCutFMD(TH2D& forwarddNdedp, double cent, Bool_t mc, TH2D* hOutliers){
  Bool_t useEvent = true;
  //if (useEvent) return useEvent;
  Int_t nBadBins = 0;
  Int_t phibins = forwarddNdedp.GetNbinsY();
  Double_t totalFMDpar = 0;

  for (Int_t etaBin = 1; etaBin <= forwarddNdedp.GetNbinsX(); etaBin++) {

    Double_t eta = forwarddNdedp.GetXaxis()->GetBinCenter(etaBin);
    Double_t runAvg = 0;
    Double_t avgSqr = 0;
    Double_t max = 0;
    Int_t nInAvg = 0;

    for (Int_t phiBin = 0; phiBin <= phibins; phiBin++) {
      // if (!mc){
      //   if ( fabs(eta) > 1.7) {
      //     if (phiBin == 0 && forwarddNdedp.GetBinContent(etaBin, 0) == 0) break;
      //   }
      // }
      Double_t weight = forwarddNdedp.GetBinContent(etaBin, phiBin);
      if (!weight){
        weight = 0;
      }
      totalFMDpar += weight;

      // We calculate the average Nch per. bin
      avgSqr += weight*weight;
      runAvg += weight;
      nInAvg++;
      if (weight == 0) continue;
      if (weight > max) {
        max = weight;
      }
    } // End of phi loop

    // Outlier cut calculations
    double fSigmaCut = 4.0;
    if (nInAvg > 0) {
      runAvg /= nInAvg;
      avgSqr /= nInAvg;
      Double_t stdev = (nInAvg > 1 ? TMath::Sqrt(nInAvg/(nInAvg-1))*TMath::Sqrt(avgSqr - runAvg*runAvg) : 0);
      Double_t nSigma = (stdev == 0 ? 0 : (max-runAvg)/stdev);
      hOutliers->Fill(cent,nSigma);
      //std::cout << "sigma = " << nSigma << std::endl;
      if (fSigmaCut > 0. && nSigma >= fSigmaCut && cent < 60) nBadBins++;
      else nBadBins = 0;
      // We still finish the loop, for fOutliers to make sense,
      // but we do no keep the event for analysis
      if (nBadBins > 3) useEvent = false;
     //if (nBadBins > 3) std::cout << "NUMBER OF BAD BINS > 3" << std::endl;
    }
  } // End of eta bin
  if (totalFMDpar < 10) useEvent = false;

  return useEvent;
}


Double_t AliForwardFlowUtil::GetZ(){
  if (mc) return fMCevent->GetPrimaryVertex()->GetZ();
  else return fevent->GetPrimaryVertex()->GetZ();
}


Double_t AliForwardFlowUtil::GetCentrality(TString centrality_estimator){
  // Get MultSelection
  AliMultSelection *MultSelection;

  MultSelection  = (AliMultSelection*)fevent->FindListObject("MultSelection");

  return MultSelection->GetMultiplicityPercentile(centrality_estimator);
}


void AliForwardFlowUtil::FillFromTrackrefs(TH2D*& cen, TH2D*& fwd) const
{

  Int_t nTracks = fMCevent->Stack()->GetNtrack();

  for (Int_t iTr = 0; iTr < nTracks; iTr++) {
      AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (p->Charge() == 0) continue;

    for (Int_t iTrRef = 0; iTrRef < p->GetNumberOfTrackReferences(); iTrRef++) {
      AliTrackReference* ref = p->GetTrackReference(iTrRef);
      // Check hit on FMD
      if (!ref) continue;
      if (AliTrackReference::kTPC == ref->DetectorId()){
        Double_t x      = ref->X() - fMCevent->GetPrimaryVertex()->GetX();
        Double_t y      = ref->Y() - fMCevent->GetPrimaryVertex()->GetY();
        Double_t z      = ref->Z() - fMCevent->GetPrimaryVertex()->GetZ();
        Double_t rr     = TMath::Sqrt(x * x + y * y);
        Double_t thetaR = TMath::ATan2(rr, z);
        Double_t phiR   = TMath::ATan2(y,x);

        if (phiR < 0) {
          phiR += 2*TMath::Pi();
        }
        if (thetaR < 0) {
          thetaR += 2*TMath::Pi();
        }
        cen->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),phiR);
        if (dodNdeta) dNdeta->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),1);

      }
      else if (AliTrackReference::kFMD == ref->DetectorId()) {
        Double_t x      = ref->X() - fMCevent->GetPrimaryVertex()->GetX();
        Double_t y      = ref->Y() - fMCevent->GetPrimaryVertex()->GetY();
        Double_t z      = ref->Z() - fMCevent->GetPrimaryVertex()->GetZ();
        Double_t rr     = TMath::Sqrt(x * x + y * y);
        Double_t thetaR = TMath::ATan2(rr, z);
        Double_t phiR   = TMath::ATan2(y,x);

        if (phiR < 0) {
          phiR += 2*TMath::Pi();
        }
        if (thetaR < 0) {
          thetaR += 2*TMath::Pi();
        }
        fwd->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),phiR);
        if (dodNdeta) dNdeta->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),1);

      }
    }
  }
}



void AliForwardFlowUtil::FillFromTrackrefs(TH2D*& fwd) const
{

  Int_t nTracks = fMCevent->Stack()->GetNtrack();

  for (Int_t iTr = 0; iTr < nTracks; iTr++) {
      AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (p->Charge() == 0) continue;

    for (Int_t iTrRef = 0; iTrRef < p->GetNumberOfTrackReferences(); iTrRef++) {
      AliTrackReference* ref = p->GetTrackReference(iTrRef);
      // Check hit on FMD
      if (!ref) continue;

      else if (AliTrackReference::kFMD == ref->DetectorId()) {
        Double_t x      = ref->X() - fMCevent->GetPrimaryVertex()->GetX();
        Double_t y      = ref->Y() - fMCevent->GetPrimaryVertex()->GetY();
        Double_t z      = ref->Z() - fMCevent->GetPrimaryVertex()->GetZ();
        Double_t rr     = TMath::Sqrt(x * x + y * y);
        Double_t thetaR = TMath::ATan2(rr, z);
        Double_t phiR   = TMath::ATan2(y,x);

        if (phiR < 0) {
          phiR += 2*TMath::Pi();
        }
        if (thetaR < 0) {
          thetaR += 2*TMath::Pi();
        }
        fwd->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),phiR);
        if (dodNdeta) dNdeta->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),1);

      }
    }
  }
}

void AliForwardFlowUtil::FillFromPrimariesAOD(TH2D*& cen) const
{
  Int_t nTracksMC = fMCevent->GetNumberOfTracks();

  for (Int_t iTr = 0; iTr < nTracksMC; iTr++) {
    AliAODMCParticle* p = static_cast< AliAODMCParticle* >(fMCevent->GetTrack(iTr));
    if (!p->IsPhysicalPrimary()) continue;
    if (p->Charge() == 0) continue;

    Double_t eta = p->Eta();
    if (TMath::Abs(eta) < 1.1) {
      if (p->Pt()>=this->minpt && p->Pt()<=this->maxpt){
        cen->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
  }
}

void AliForwardFlowUtil::FillFromPrimaries(TH2D*& cen) const
{
  Int_t nTracksMC = fMCevent->GetNumberOfTracks();

  for (Int_t iTr = 0; iTr < nTracksMC; iTr++) {
    AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (!p->IsPhysicalPrimary()) continue;
    if (p->Charge() == 0) continue;

    Double_t eta = p->Eta();
    if (TMath::Abs(eta) < 1.1) {
      if (p->Pt()>=this->minpt && p->Pt()<=this->maxpt){
        cen->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
  }
}

void AliForwardFlowUtil::FillFromPrimaries(TH2D*& cen, TH2D*& fwd) const
{
  Int_t nTracksMC = fMCevent->GetNumberOfTracks();

  for (Int_t iTr = 0; iTr < nTracksMC; iTr++) {
    AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (!p->IsPhysicalPrimary()) continue;
    if (p->Charge() == 0) continue;

    Double_t eta = p->Eta();
    if (TMath::Abs(eta) < 1.1) {
      if (p->Pt()>=this->minpt && p->Pt()<=this->maxpt){
        cen->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
    if (eta < 5 /*fwd->GetXaxis()-GetXmax()*/ && eta > -3.5 /*fwd->GetXaxis()-GetXmin()*/) {
      if (TMath::Abs(eta) >= 1.7){
        fwd->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
  }
}


void AliForwardFlowUtil::FillFromPrimariesAOD(TH2D*& cen, TH2D*& fwd) const
{
  Int_t nTracksMC = fMCevent->GetNumberOfTracks();

  for (Int_t iTr = 0; iTr < nTracksMC; iTr++) {
    AliAODMCParticle* p = static_cast< AliAODMCParticle* >(fMCevent->GetTrack(iTr));
    if (!p->IsPhysicalPrimary()) continue;
    if (p->Charge() == 0) continue;

    Double_t eta = p->Eta();
    if (TMath::Abs(eta) < 1.1) {
      if (p->Pt()>=this->minpt && p->Pt()<=this->maxpt){
        cen->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
    if (eta < 5 /*fwd->GetXaxis()-GetXmax()*/ && eta > -3.5 /*fwd->GetXaxis()-GetXmin()*/) {
      if (TMath::Abs(eta) >= 1.7){
        fwd->Fill(eta,p->Phi(),1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
  }
}


void AliForwardFlowUtil::FillFromTracklets(TH2D*& cen) const {
  AliAODTracklets* aodTracklets = fAODevent->GetTracklets();

  for (Int_t i = 0; i < aodTracklets->GetNumberOfTracklets(); i++) {
    cen->Fill(aodTracklets->GetEta(i),aodTracklets->GetPhi(i), 1);
    if (dodNdeta) dNdeta->Fill(aodTracklets->GetEta(i),1);
  }
}


void AliForwardFlowUtil::FillFromTracks(TH2D*& cen, UInt_t tracktype) const {
  Int_t  iTracks(fevent->GetNumberOfTracks());
  for(Int_t i(0); i < iTracks; i++) {

  // loop  over  all  the  tracks
    AliAODTrack* track = static_cast<AliAODTrack *>(fAODevent->GetTrack(i));

    if (track->TestFilterBit(tracktype)){
      if (track->Pt() >= this->minpt && track->Pt() <= this->maxpt){
        cen->Fill(track->Eta(),track->Phi(), 1);
        if (dodNdeta) dNdeta->Fill(track->Eta(),1);
      }
    }
  }
}



void AliForwardFlowUtil::FillFromTrackrefs(TH3D*& cen, TH3D*& fwd, Double_t zvertex) const
{
  Int_t nTracks = fMCevent->Stack()->GetNtrack();

  for (Int_t iTr = 0; iTr < nTracks; iTr++) {
      AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (p->Charge() == 0) continue;

    for (Int_t iTrRef = 0; iTrRef < p->GetNumberOfTrackReferences(); iTrRef++) {
      AliTrackReference* ref = p->GetTrackReference(iTrRef);
      // Check hit on FMD
      if (!ref) continue;
      if (AliTrackReference::kTPC == ref->DetectorId()){
        Double_t x      = ref->X() - fMCevent->GetPrimaryVertex()->GetX();
        Double_t y      = ref->Y() - fMCevent->GetPrimaryVertex()->GetY();
        Double_t z      = ref->Z() - fMCevent->GetPrimaryVertex()->GetZ();
        Double_t rr     = TMath::Sqrt(x * x + y * y);
        Double_t thetaR = TMath::ATan2(rr, z);
        Double_t phiR   = TMath::ATan2(y,x);

        if (phiR < 0) {
          phiR += 2*TMath::Pi();
        }
        if (thetaR < 0) {
          thetaR += 2*TMath::Pi();
        }
        cen->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),phiR, zvertex);
        if (dodNdeta) dNdeta->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),1);

      }
      else if (AliTrackReference::kFMD == ref->DetectorId()) {
        Double_t x      = ref->X() - fMCevent->GetPrimaryVertex()->GetX();
        Double_t y      = ref->Y() - fMCevent->GetPrimaryVertex()->GetY();
        Double_t z      = ref->Z() - fMCevent->GetPrimaryVertex()->GetZ();
        Double_t rr     = TMath::Sqrt(x * x + y * y);
        Double_t thetaR = TMath::ATan2(rr, z);
        Double_t phiR   = TMath::ATan2(y,x);

        if (phiR < 0) {
          phiR += 2*TMath::Pi();
        }
        if (thetaR < 0) {
          thetaR += 2*TMath::Pi();
        }
        fwd->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),phiR, zvertex);
        if (dodNdeta) dNdeta->Fill(-TMath::Log(TMath::Tan(thetaR / 2)),1);
      }
    }
  }
}


void AliForwardFlowUtil::FillFromPrimaries(TH3D*& cen, TH3D*& fwd, Double_t zvertex) const
{
  Int_t nTracksMC = fMCevent->GetNumberOfTracks();

  for (Int_t iTr = 0; iTr < nTracksMC; iTr++) {
    AliMCParticle* p = static_cast< AliMCParticle* >(fMCevent->GetTrack(iTr));
    if (!p->IsPhysicalPrimary()) continue;
    if (p->Charge() == 0) continue;

    Double_t eta = p->Eta();
    if (TMath::Abs(eta) < 1.1) {
      if (p->Pt()>=this->minpt && p->Pt()<=this->maxpt){
        cen->Fill(eta,p->Phi(),zvertex,1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
    if (eta < 5 /*fwd->GetXaxis()-GetXmax()*/ && eta > -3.5 /*fwd->GetXaxis()-GetXmin()*/) {
      if (TMath::Abs(eta) >= 1.7){
        fwd->Fill(eta,p->Phi(),zvertex,1);
        if (dodNdeta) dNdeta->Fill(eta,1);
      }
    }
  }
}


void AliForwardFlowUtil::FillFromTracklets(TH3D*& cen, Double_t zvertex) const {
  AliAODTracklets* aodTracklets = fAODevent->GetTracklets();

  for (Int_t i = 0; i < aodTracklets->GetNumberOfTracklets(); i++) {
    cen->Fill(aodTracklets->GetEta(i),aodTracklets->GetPhi(i),zvertex, 1);
    if (dodNdeta) dNdeta->Fill(aodTracklets->GetEta(i),1);
  }
}


void AliForwardFlowUtil::FillFromTracks(TH3D*& cen, Int_t tracktype, Double_t zvertex) const {
  Int_t  iTracks(fevent->GetNumberOfTracks());
  for(Int_t i(0); i < iTracks; i++) {

  // loop  over  all  the  tracks
    AliAODTrack* track = static_cast<AliAODTrack *>(fAODevent->GetTrack(i));
    if (track->TestFilterBit(tracktype)){
      if (track->Pt() >= this->minpt && track->Pt() <= this->maxpt){
        cen->Fill(track->Eta(),track->Phi(), zvertex, 1);
        if (dodNdeta) dNdeta->Fill(track->Eta(),1);
      }
    }
  }
}



AliTrackReference* AliForwardFlowUtil::IsHitFMD(AliMCParticle* p) {
  //std::cout << "p->GetNumberOfTrackReferences() = " << p->GetNumberOfTrackReferences() << std::endl;
  for (Int_t iTrRef = 0; iTrRef < p->GetNumberOfTrackReferences(); iTrRef++) {
    AliTrackReference* ref = p->GetTrackReference(iTrRef);
    // Check hit on FMD
    if (!ref || AliTrackReference::kFMD != ref->DetectorId()) {
      continue;
    }
    else {
      return ref;
    }
  }
  return 0x0;
}

AliTrackReference* AliForwardFlowUtil::IsHitTPC(AliMCParticle* p) {
  for (Int_t iTrRef = 0; iTrRef < p->GetNumberOfTrackReferences(); iTrRef++) {
    AliTrackReference* ref = p->GetTrackReference(iTrRef);
    // Check hit on FMD
    if (!ref || AliTrackReference::kTPC != ref->DetectorId()) {
      continue;
    }
    else {
      return ref;
    }
  }
  return 0x0;
}


void AliForwardFlowUtil::MakeFakeHoles(TH2D& forwarddNdedp){
  for (Int_t etaBin = 125; etaBin <= 137; etaBin++){
    forwarddNdedp.SetBinContent(etaBin,17, 0.0);
    forwarddNdedp.SetBinContent(etaBin,18, 0.0);
  }
  for (Int_t etaBin = 168; etaBin <= 185; etaBin++){
    forwarddNdedp.SetBinContent(etaBin,14, 0.0);
  }
}