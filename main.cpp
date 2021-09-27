
/* Geant4 */
#include <G4NistManager.hh>

/* ROOT */
#include <TXMLEngine.h> /* https://root.cern/doc/master/group__tutorial__xml.html */

using namespace std;

void WriteMaterialsXML(const string &filename = "materials.xml") {

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
        for (int i = 0; i < element->GetNumberOfIsotopes(); i++) {
            auto isotope = element->GetIsotope(i);

            auto elementNode = xml.NewChild(materials, nullptr, "element");
            xml.NewAttr(elementNode, nullptr, "name", elementName.c_str());

            auto fractionNode = xml.NewChild(elementNode, nullptr, "fraction");
            xml.NewAttr(fractionNode, nullptr, "n", to_string(element->GetRelativeAbundanceVector()[i]).c_str());
            xml.NewAttr(fractionNode, nullptr, "ref", isotope->GetName().c_str());
        }

        if (elementName == "Cf") break;// can't go further
    }

    /* Materials */
    for (const auto &materialName : nistManager->GetNistMaterialNames()) {
        auto material = nistManager->FindOrBuildMaterial(materialName);

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

        auto densityNode = xml.NewChild(materialNode, nullptr, "T");
        xml.NewAttr(densityNode, nullptr, "unit", "g/cm3");
        xml.NewAttr(densityNode, nullptr, "value", to_string(material->GetDensity() / CLHEP::gram * CLHEP::cm3).c_str());

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

    WriteMaterialsXML(filename);
}