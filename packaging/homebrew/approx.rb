class Approx < Formula
  desc "Non-interactive fuzzy stream filter and ranker"
  homepage "https://github.com/riccivr/approx"
  url "https://github.com/riccivr/approx/archive/refs/tags/v1.1.0.tar.gz"
  sha256 "SKIP" # Replace with tag tarball sha256
  license "MIT"
  head "https://github.com/riccivr/approx.git", branch: "main"

  def install
    system "make", "PREFIX=#{prefix}", "MANPREFIX=#{man}", "install"
  end

  test do
    assert_equal "match line\n", pipe_output("#{bin}/approx match", "match line\nnomatch\n")
    assert_equal "0.80\trecieve\n", pipe_output("#{bin}/approx -s recieve", "recieve\n")
  end
end
