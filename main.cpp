
/* Geant4 */
#include <G4NistManager.hh>
/* ROOT */
#include <TString.h>
#include <TXMLEngine.h> /* https://root.cern/doc/master/group__tutorial__xml.html */

#include <cstdio>
#include <iostream>

using namespace std;

#define SCIENTIFIC(decimal) (Form("%1.10g", decimal))

G4Material* CopyMaterial(const string& name, G4Material* material) {
    return new G4Material(name, material->GetDensity(), material, material->GetState(), material->GetTemperature(), material->GetPressure());
}

void GenerateUserMaterials(vector<G4Material*>* container) {
    auto nistManager = G4NistManager::Instance();

    /* Gases */
    // Quenchers
    // https://en.wikipedia.org/wiki/Isobutane
    auto isobutane = new G4Material("Isobutane", 2.51 * CLHEP::kg / CLHEP::m3, 2, G4State::kStateGas);
    isobutane->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("C"), 4);
    isobutane->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("H"), 10);
    container->push_back(isobutane);

    // https://en.wikipedia.org/wiki/Trimethylamine
    auto tma = new G4Material("TMA", 627.0 * CLHEP::kg / CLHEP::m3, 3, G4State::kStateGas);
    tma->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("C"), 3);
    tma->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("H"), 9);
    tma->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("N"), 1);
    container->push_back(tma);

    // Gas mixtures
    double gasMixturePressure = 1.4 * CLHEP::bar;
    double gasMixtureTemperature = NTP_Temperature;
    double gasMixtureQuencherFraction = 0.02;

    for (const string& quencher : {"Isobutane", "TMA"}) {
        auto quencherMaterial = nistManager->FindOrBuildMaterial(quencher);
        for (const string& target : {"Argon", "Xenon", "Neon"}) {
            G4Material* targetMaterial;
            if (target == "Argon") {
                targetMaterial = nistManager->FindOrBuildMaterial("G4_Ar");
            } else if (target == "Xenon") {
                targetMaterial = nistManager->FindOrBuildMaterial("G4_Xe");
            } else if (target == "Neon") {
                targetMaterial = nistManager->FindOrBuildMaterial("G4_Ne");
            }

            auto gasMixture = new G4Material(
                    "GasMixture-" + target + string(SCIENTIFIC(gasMixtureQuencherFraction * 100)) + "%" + quencher + string(SCIENTIFIC(gasMixturePressure / CLHEP::bar)) + "Bar",
                    (gasMixturePressure / CLHEP::STP_Pressure) *
                            ((1.0 - gasMixtureQuencherFraction) * targetMaterial->GetDensity() + gasMixtureQuencherFraction * quencherMaterial->GetDensity()),
                    2, G4State::kStateGas, gasMixtureTemperature, gasMixturePressure);

            gasMixture->AddMaterial(targetMaterial, 1.00 - gasMixtureQuencherFraction);
            gasMixture->AddMaterial(quencherMaterial, gasMixtureQuencherFraction);
            container->push_back(gasMixture);
        }
    }
    // Vacuum (same as G4_Galactic)
    auto vacuum = CopyMaterial("Vacuum", nistManager->FindOrBuildMaterial("G4_Galactic"));
    container->push_back(vacuum);
    // Scintillators
    auto BC408 = new G4Material("BC408", 1.03 * CLHEP::gram / CLHEP::cm3, 2, kStateSolid);
    BC408->AddMaterial(nistManager->FindOrBuildMaterial("G4_H"), 0.085000);
    BC408->AddMaterial(nistManager->FindOrBuildMaterial("G4_C"), 0.915000);
    container->push_back(BC408);
    // Liquid Scintillator with additives
    auto HC = new G4Material("HC", 1.00 * CLHEP::gram / CLHEP::cm3, 2, kStateLiquid);
    HC->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("H"), 1);
    HC->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("C"), 1);
    container->push_back(HC);

    const auto lead = nistManager->FindOrBuildMaterial("G4_Pb");
    const auto cadmium = nistManager->FindOrBuildMaterial("G4_Cd");
    for (const double fractionLead : {0.0, 0.01, 0.05, 0.10, 0.20, 0.50}) {
        auto scintillatorWithLead = new G4Material("LiquidScintillator-HC+" + string(SCIENTIFIC(fractionLead * 100)) + "%Pb",
                                                   HC->GetDensity() * (1 - fractionLead) + lead->GetDensity() * fractionLead, 2, kStateLiquid);
        scintillatorWithLead->AddMaterial(HC, (1 - fractionLead));
        scintillatorWithLead->AddMaterial(lead, fractionLead);
        container->push_back(scintillatorWithLead);

        const double fractionCadmium = 0.001;
        auto scintillatorWithLeadAndCadmium = new G4Material(
                "LiquidScintillator-HC+" + string(SCIENTIFIC(fractionLead * 100)) + "%Pb+" + string(SCIENTIFIC(fractionCadmium * 100)) + "%Cd",
                HC->GetDensity() * (1 - fractionLead - fractionCadmium) + lead->GetDensity() * fractionLead + cadmium->GetDensity() * fractionCadmium, 3, kStateLiquid);
        scintillatorWithLeadAndCadmium->AddMaterial(HC, (1 - fractionLead - fractionCadmium));
        scintillatorWithLeadAndCadmium->AddMaterial(lead, fractionLead);
        scintillatorWithLeadAndCadmium->AddMaterial(cadmium, fractionCadmium);
        container->push_back(scintillatorWithLeadAndCadmium);
    }
}

void WriteMaterialsXML(const string& filename = "materials.xml", vector<G4Material*>* container = {}) {

    auto nistManager = G4NistManager::Instance();

    TXMLEngine xml;

    auto materials = xml.NewChild(nullptr, nullptr, "materials");

    for (const auto& elementName : nistManager->GetNistElementNames()) {

        auto element = nistManager->FindOrBuildElement(elementName, true);
        if (!element) { continue; }

        /* Isotopes */
        for (int i = 0; i < element->GetNumberOfIsotopes(); i++) {
            auto isotope = element->GetIsotope(i);

            auto isotopeNode = xml.NewChild(materials, nullptr, "isotope");
            xml.NewAttr(isotopeNode, nullptr, "N", to_string(isotope->GetN()).c_str());
            xml.NewAttr(isotopeNode, nullptr, "Z", to_string(isotope->GetZ()).c_str());
            xml.NewAttr(isotopeNode, nullptr, "name", isotope->GetName().c_str());

            auto atomNode = xml.NewChild(isotopeNode, nullptr, "atom");
            xml.NewAttr(atomNode, nullptr, "unit", "g/mole");
            xml.NewAttr(atomNode, nullptr, "value", to_string(isotope->GetA() / CLHEP::gram).c_str());
        }

        /* Element */
        auto elementNode = xml.NewChild(materials, nullptr, "element");
        xml.NewAttr(elementNode, nullptr, "name", elementName.c_str());

        for (int i = 0; i < element->GetNumberOfIsotopes(); i++) {
            auto isotope = element->GetIsotope(i);

            auto fractionNode = xml.NewChild(elementNode, nullptr, "fraction");
            xml.NewAttr(fractionNode, nullptr, "n", SCIENTIFIC(element->GetRelativeAbundanceVector()[i]));
            xml.NewAttr(fractionNode, nullptr, "ref", isotope->GetName().c_str());
        }

        if (elementName == "Cf") break;// can't go further
    }

    /* Materials */
    // Merge NIST materials with UserDefinedMaterials if they exist
    vector<string> materialNames;
    cout << "Writing " + to_string(nistManager->GetNistMaterialNames().size()) + " NIST materials." << endl;
    for (const string& materialName : nistManager->GetNistMaterialNames()) { materialNames.push_back(materialName); }
    if (!container->empty()) {
        cout << "Writing " + to_string(container->size()) + " User defined materials:" << endl;
        for (const auto material : *container) {
            const auto& name = material->GetName();
            cout << "   " << name << endl;

            if (std::find(materialNames.begin(), materialNames.end(), name) != materialNames.end()) {
                cout << "ERROR: duplicate material '" << name << "'";
                exit(1);
            }

            materialNames.push_back(name);
        }
    }

    for (const auto& materialName : materialNames) {
        auto material = nistManager->FindOrBuildMaterial(materialName);// This will also find materials already defined (on the heap)

        string state = "undefined";
        if (material->GetState() == G4State::kStateGas) {
            state = "gas";
        } else if (material->GetState() == G4State::kStateLiquid) {
            state = "liquid";
        }
        if (material->GetState() == G4State::kStateSolid) { state = "solid"; }

        auto materialNode = xml.NewChild(materials, nullptr, "material");
        xml.NewAttr(materialNode, nullptr, "name", material->GetName().c_str());
        xml.NewAttr(materialNode, nullptr, "state", state.c_str());

        auto temperatureNode = xml.NewChild(materialNode, nullptr, "T");
        xml.NewAttr(temperatureNode, nullptr, "unit", "K");
        xml.NewAttr(temperatureNode, nullptr, "value", SCIENTIFIC(material->GetTemperature() / CLHEP::kelvin));

        auto meeNode = xml.NewChild(materialNode, nullptr, "MEE");
        xml.NewAttr(meeNode, nullptr, "unit", "eV");
        xml.NewAttr(meeNode, nullptr, "value", SCIENTIFIC(material->GetIonisation()->GetMeanExcitationEnergy() / CLHEP::eV));

        auto densityNode = xml.NewChild(materialNode, nullptr, "D");
        if (material->GetState() != G4State::kStateGas) {
            xml.NewAttr(densityNode, nullptr, "unit", "g/cm3");
            xml.NewAttr(densityNode, nullptr, "value", SCIENTIFIC(material->GetDensity() / CLHEP::gram * CLHEP::cm3));
        } else {
            xml.NewAttr(densityNode, nullptr, "unit", "kg/m3");
            xml.NewAttr(densityNode, nullptr, "value", SCIENTIFIC(material->GetDensity() / CLHEP::kg * CLHEP::m3));
            // Add pressure for gases
            auto pressureNode = xml.NewChild(materialNode, nullptr, "P");
            xml.NewAttr(pressureNode, nullptr, "unit", "bar");
            //xml.NewAttr(pressureNode, nullptr, "value", to_string(material->GetPressure() / CLHEP::bar).c_str());
            xml.NewAttr(pressureNode, nullptr, "value", SCIENTIFIC(material->GetPressure() / CLHEP::bar));
        }

        for (int i = 0; i < material->GetNumberOfElements(); i++) {
            auto fractionNode = xml.NewChild(materialNode, nullptr, "fraction");
            xml.NewAttr(fractionNode, nullptr, "n", to_string(material->GetFractionVector()[i]).c_str());
            xml.NewAttr(fractionNode, nullptr, "ref", material->GetElement(i)->GetName().c_str());
        }
    }

    auto document = xml.NewDoc();
    xml.DocSetRootElement(document, materials);
    xml.SaveDoc(document, filename.c_str());
    xml.FreeDoc(document);// release memory
}

int main() {

    string filename = "materials.xml";

    cout << "Generating '" << filename << "' file." << endl;

    auto* userMaterials = new vector<G4Material*>();

    GenerateUserMaterials(userMaterials);

    WriteMaterialsXML(filename, userMaterials);
}