#ifndef CHARTNAVIGATION_XPLANE_HPP
#define CHARTNAVIGATION_XPLANE_HPP

namespace eydTest
{
// 用于测试TCAS
class XPlaneUdp {
    public:
        struct DatarefIndex {
            DatarefIndex () : idx(0) {}
            explicit DatarefIndex (const size_t index) : idx(index) {}
            [[nodiscard]] size_t getIdx () const { return idx; }
            private:
                size_t idx;
        };

        XPlaneUdp ();
        void setCallback (const std::function<void  (bool)> &callbackFunc);
        DatarefIndex addDatarefArray (const std::string &dataref, int length, int32_t freq = 1);
        template <typename T>
        bool getDataref (const DatarefIndex &dataref, T &container, float defaultValue = 0);
};

template <typename T>
bool XPlaneUdp::getDataref (const DatarefIndex &dataref, T &container, float defaultValue) {}
}

#endif //CHARTNAVIGATION_XPLANE_HPP
