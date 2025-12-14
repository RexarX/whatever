#include <client/app/frame.hpp>

#include <opencv2/imgproc.hpp>

namespace client {

Frame Frame::Clone() const {
  Frame result;
  result.mat_ = mat_.clone();
  return result;
}

Frame Frame::ConvertColor(int code) const {
  Frame result;
  cv::cvtColor(mat_, result.mat_, code);
  return result;
}

Frame Frame::Resize(int width, int height) const {
  Frame result;
  cv::resize(mat_, result.mat_, cv::Size(width, height));
  return result;
}

}  // namespace client
