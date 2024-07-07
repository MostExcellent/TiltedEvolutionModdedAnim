#include <Structs/AnimationVariables.h>
#include <TiltedCore/Serialization.hpp>
#include <client/Utils.h>

bool AnimationVariables::operator==(const AnimationVariables& acRhs) const noexcept
{
    return Booleans == acRhs.Booleans && Integers == acRhs.Integers && Floats == acRhs.Floats;
}

bool AnimationVariables::operator!=(const AnimationVariables& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

// std::vector<bool> implementation is unspecified, but often packed reasonably.
// The spec does not guarantee contiguous memory, though, so somewhat laborious 
// translation needed. Should be better than winding down several layers to 
// TiltedPhoques::Serialization::WriteBool, though.
//
// I don't know if array-style indexing is better with the compiler
void AnimationVariables::VectorBool_to_String(const Vector<bool>& bools, TiltedPhoques::String& chars) const
{
    chars.resize((bools.size() + 7) >> 3, 0);

    for (size_t i = 0; i < bools.size(); ++i)
    {
        if (bools[i])
            chars[i >> 3] |= 1 << (i & 7);
    }
}

void AnimationVariables::String_to_VectorBool(const TiltedPhoques::String& chars, Vector<bool>& bools)
{
    bools.resize(chars.size() * 8);

    for (size_t i = 0; i < chars.size(); ++i)
    {
        unsigned char byte = chars[i];
        for (size_t j = 0; j < 8; ++j)
        {
            bools[i * 8 + j] = (byte >> (7 - j)) & 1;
        }
    }
}


void AnimationVariables::Load(std::istream& aInput)
{
    // Booleans are bitpacked and a bit different, not guaranteed contiguous.
    TiltedPhoques::String chars((Booleans.size() + 7) >> 3, 0);

    aInput.read(reinterpret_cast<char*>(chars.data()), chars.size());
    String_to_VectorBool(chars, Booleans);
    aInput.read(reinterpret_cast<char*>(Integers.data()), Integers.size() * sizeof(uint32_t));
    aInput.read(reinterpret_cast<char*>(Floats.data()), Floats.size() * sizeof(float));
}

void AnimationVariables::Save(std::ostream& aOutput) const
{
    // Booleans bitpacked and not guaranteed contiguous.
    TiltedPhoques::String chars;
    VectorBool_to_String(Booleans, chars);
 
    aOutput.write(reinterpret_cast<const char*>(chars.data()), chars.size());
    aOutput.write(reinterpret_cast<const char*>(Integers.data()), Integers.size() * sizeof(uint32_t));
    aOutput.write(reinterpret_cast<const char*>(Floats.data()), Floats.size() * sizeof(float));
}

// Wire format description.
//
// Sends 3 VarInts, the count of Booleans, Integers and Floats, in that order. Then sends a bitstream of the
// sum of those counts. For the Booleans, these represent the bit values for the Booleans. For the Integers and
// Floats, it represents a truth table for whether the value has changed. If values HAVE changed, they follow on 
// the stream.
//
//
void AnimationVariables::GenerateDiff(const AnimationVariables& aPrevious, TiltedPhoques::Buffer::Writer& aWriter) const
{
    const size_t sizeChangedVector = Booleans.size() + Integers.size() + Floats.size();
    auto changedVector = Booleans;
    changedVector.reserve(sizeChangedVector);

    for (size_t i = 0; i < Integers.size(); i++)
        changedVector.push_back(aPrevious.Integers.size() != Integers.size() || aPrevious.Integers[i] != Integers[i]);
    for (size_t i = 0; i < Floats.size(); i++)
        changedVector.push_back(
            aPrevious.Floats.size() != Floats.size() || !Utils::IsNearlyEqual(aPrevious.Floats[i], Floats[i]));

    // Now serialize: VarInts Booleans.size(), Integers.size(), Floats.size(),
    // then the change table bits, then changed Integers, then changed Floats.
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Booleans.size());
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Integers.size());
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Floats.size());

    TiltedPhoques::String chars;
    VectorBool_to_String(changedVector, chars);
    TiltedPhoques::Serialization::WriteString(aWriter, chars);

    auto biter = changedVector.begin() + Booleans.size();
    for (unsigned int Integer : Integers)
        if (*biter++)
            TiltedPhoques::Serialization::WriteVarInt(aWriter, Integer & 0xFFFFFFFF);
    for (float Float : Floats)
        if (*biter++)
            TiltedPhoques::Serialization::WriteFloat(aWriter, Float);
}

// Reads 3 VarInts that represent the size of the Booleans, Integers and Floats.
// That's followed by a bitstream in a string of the Booleans values combined
// with a Changed? truth table for Integers and Floats.
// The Changed? table is scanned and for each true bit, the corresponsing Integer
// or Float is deserialized.
// 
void AnimationVariables::ApplyDiff(TiltedPhoques::Buffer::Reader& aReader)
{
    size_t booleansSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    size_t integersSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    size_t floatsSize   = TiltedPhoques::Serialization::ReadVarInt(aReader);
    if (Integers.size() != integersSize)
        Integers.assign(integersSize, 0);
    if (Floats.size() != floatsSize)
        Floats.assign(floatsSize, 0.f);
    
    TiltedPhoques::Vector<bool> changedVector(booleansSize + integersSize + floatsSize);
    auto chars = TiltedPhoques::Serialization::ReadString(aReader);
    String_to_VectorBool(chars, changedVector);

    Booleans.assign(changedVector.begin(), changedVector.begin() + booleansSize);

    auto biter = changedVector.begin() + booleansSize;
    for (size_t i = 0; i < integersSize; i++)
        if (*biter++)
            Integers[i] = TiltedPhoques::Serialization::ReadVarInt(aReader);
    for (size_t i = 0; i < floatsSize; i++)
        if (*biter++)
            Floats[i] = TiltedPhoques::Serialization::ReadFloat(aReader);
}
