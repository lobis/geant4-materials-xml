
/* Geant4 */
#include <G4NistManager.hh>
/* ROOT */
#include <TXMLEngine.h> /* https://root.cern/doc/master/group__tutorial__xml.html */

#include <stdio.h>

using namespace std;

void GenerateUserMaterials(vector<G4Material *> *container) {
    auto nistManager = G4NistManager::Instance();

    // Compound materials
    auto isobutane = new G4Material("Isobutane", 2.51 * CLHEP::kg / CLHEP::m3, 2, G4State::kStateGas);
    container->push_back(isobutane);

    isobutane->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("C"), 4);
    isobutane->AddElementByNumberOfAtoms(nistManager->FindOrBuildElement("H"), 10);

    // Gas
    double gasMixturePressure = 1.4 * CLHEP::bar;
    double gasMixtureTemperature = NTP_Temperature;
    double gasMixtureQuencherFraction = 0.02;

    auto gasMixtureTarget = new G4Material("ArgonTarget", (1.0 - gasMixtureQuencherFraction) * nistManager->FindOrBuildMaterial("G4_Ar")->GetDensity() * gasMixturePressure / CLHEP::STP_Pressure, 1,
                                           nistManager->FindOrBuildMaterial("G4_Ar")->GetState(), gasMixtureTemperature, gasMixturePressure);
    gasMixtureTarget->AddElementByMassFraction(nistManager->FindOrBuildElement("Ar"), 1.00);

    auto gasMixtureQuencher = new G4Material("IsobutaneQuencher", gasMixtureQuencherFraction * isobutane->GetDensity() * gasMixturePressure / CLHEP::STP_Pressure, 1, isobutane->GetState(),
                                             gasMixtureTemperature, gasMixturePressure);
    gasMixtureQuencher->AddMaterial(isobutane, 1.00);

    auto gasMixture = new G4Material("GasMixture-Argon2.00%Isobutane-1.4Bar", gasMixtureTarget->GetDensity() + gasMixtureQuencher->GetDensity(), 2, G4State::kStateGas, gasMixtureTemperature,
                                     gasMixturePressure);
    container->push_back(gasMixture);

    gasMixture->AddMaterial(gasMixtureTarget, 1.00 - gasMixtureQuencherFraction);
    gasMixture->AddMaterial(gasMixtureQuencher, gasMixtureQuencherFraction);

    // Compound
    auto BC408 = new G4Material("BC408", 1.03 * CLHEP::gram / CLHEP::cm3, 2, kStateSolid);
    container->push_back(BC408);

    BC408->AddMaterial(nistManager->FindOrBuildMaterial("G4_H"), 0.085000);
    BC408->AddMaterial(nistManager->FindOrBuildMaterial("G4_C"), 0.915000);
}

void WriteMaterialsXML(const string &filename = "materials.xml", vector<G4Material *> *container = {}) {

    auto nistManager = G4NistManager::Instance();

    TXMLEngine xml;

    auto materials = xml.NewChild(nullptr, nullptr, "materials");

    for (const auto &elementName : nistManager->GetNistElementNames()) {

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
            xml.NewAttr(fractionNode, nullptr, "n", to_string(element->GetRelativeAbundanceVector()[i]).c_str());
            xml.NewAttr(fractionNode, nullptr, "ref", isotope->GetName().c_str());
        }

        if (elementName == "Cf") break;// can't go further
    }

    /* Materials */
    // Merge NIST materials with UserDefinedMaterials if they exist
    vector<string> materialNames;
    for (const string &materialName : nistManager->GetNistMaterialNames()) { materialNames.push_back(materialName); }
    for (const auto material : *container) {
        const auto &name = material->GetName();

        if (std::find(materialNames.begin(), materialNames.end(), name) != materialNames.end()) {
            cout << "ERROR: duplicate material '" << name << "'";
            exit(1);
        }

        materialNames.push_back(name);
    }

    for (const auto &materialName : materialNames) {
        auto material = nistManager->FindOrBuildMaterial(materialName);// This will also find materials already defined (on the heap)

        string state = "undefined";
        if (material->GetState() == G4State::kStateGas) {
            state = "gas";
        } else if (material->GetState() == G4State::kStateLiquid) {
            state = "liquid";
        }
        if (material->GetState() == G4State::kStateSolid) {
            state = "solid";
        }

        auto materialNode = xml.NewChild(materials, nullptr, "material");
        xml.NewAttr(materialNode, nullptr, "name", material->GetName().c_str());
        xml.NewAttr(materialNode, nullptr, "state", state.c_str());

        auto temperatureNode = xml.NewChild(materialNode, nullptr, "T");
        xml.NewAttr(temperatureNode, nullptr, "unit", "K");
        xml.NewAttr(temperatureNode, nullptr, "value", to_string(material->GetTemperature() / CLHEP::kelvin).c_str());

        auto meeNode = xml.NewChild(materialNode, nullptr, "MEE");
        xml.NewAttr(meeNode, nullptr, "unit", "eV");
        xml.NewAttr(meeNode, nullptr, "value", to_string(material->GetIonisation()->GetMeanExcitationEnergy() / CLHEP::eV).c_str());

        auto densityNode = xml.NewChild(materialNode, nullptr, "D");
        if (material->GetState() != G4State::kStateGas) {
            xml.NewAttr(densityNode, nullptr, "unit", "g/cm3");
            xml.NewAttr(densityNode, nullptr, "value", to_string(material->GetDensity() / CLHEP::gram * CLHEP::cm3).c_str());
        } else {
            xml.NewAttr(densityNode, nullptr, "unit", "kg/m3");
            xml.NewAttr(densityNode, nullptr, "value", to_string(material->GetDensity() / CLHEP::kg * CLHEP::m3).c_str());
            // Add pressure for gases
            auto pressureNode = xml.NewChild(materialNode, nullptr, "P");
            xml.NewAttr(pressureNode, nullptr, "unit", "bar");
            xml.NewAttr(pressureNode, nullptr, "value", to_string(material->GetPressure() / CLHEP::bar).c_str());
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

    auto *userMaterials = new vector<G4Material *>();

    GenerateUserMaterials(userMaterials);

    WriteMaterialsXML(filename, userMaterials);
}